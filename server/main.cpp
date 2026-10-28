#include "cachelet/cache.hpp"
#include "cachelet/dispatch.hpp"
#include "cachelet/resp.hpp"
#include "cachelet/aof.hpp"

#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>

#include <cstdio>
#include <cerrno>
#include <cstring>

#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>

#include <sys/epoll.h>

using namespace Cachelet;

struct Client {
    std::string input_buffer;
    std::string output_buffer;
    bool remove_after_send = false;
};

namespace {
    
void to_lower_string(std::string &str){
    for(char &c : str){
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c))); 
    }
}
    

// when we aren't guaranteed to be able to send all the bytes (could have a partial send)
// returns the number of bytes LEFT, except for -1, which indicates a send failure
ssize_t try_send(int client_sock, Client &client){
    ssize_t original_byte_count = client.output_buffer.size();
    while(!client.output_buffer.empty()){
        ssize_t bytes_sent = send(client_sock, client.output_buffer.data(), client.output_buffer.size(), 
        MSG_NOSIGNAL); // Prevents program termination on writing to a closed socket, instead returns an error
        if(bytes_sent > 0){
            original_byte_count -= bytes_sent;
            client.output_buffer.erase(0, bytes_sent);
            continue;
        } else if(bytes_sent == -1){
            if(errno == EAGAIN || errno == EWOULDBLOCK){
            // kernel full, can't send any more. wait for EPOLLOUT signal
                return original_byte_count;
            } else {
                perror("send");
                return -1;
            }
        } else {
            // in theory only reachable when 0 bytes sent? but should never be the case
            // (alr checked for empty buffer, and full buffer -> EAGAIN, so no scenario where this could happen)
            return original_byte_count;
        }
    }
    return 0;
}

// whenever we try to send bytes to the client, pass through this logic
// it attempts to send bytes and responds according to the result, including setting the EPOLLOUT flag
bool handle_send(int epfd, int client_sock, Client &client){
    ssize_t bytes_left = try_send(client_sock, client); // note try_send returns bytes remaining
    epoll_event ev{};
    ev.events = (client.remove_after_send ? 0 : EPOLLIN); // we may or may not mark EPOLLOUT depending on if we have pending output
    // note that for clients we want to close asap (indicated by the flag) we shouldn't waste CPU on trying to read input that will just return early
    ev.data.fd = client_sock;
    
    if(bytes_left == -1){ // failure to send bytes
        return false;
    } else if(bytes_left > 0){
        ev.events |= EPOLLOUT;
        if(epoll_ctl(epfd, EPOLL_CTL_MOD, client_sock, &ev) == -1){
            perror("epoll_ctl");
            return false;
        }
    } else { // all bytes sent! e.g. no bytes remaining
        // remove EPOLLOUT flag since we don't have "pending" sends anymore
        if(epoll_ctl(epfd, EPOLL_CTL_MOD, client_sock, &ev) == -1){
            perror("epoll_ctl");
            return false;
        }
    }
    return true;
}

// Returns false if we encounter a fatal error, incomplete sends are now fine
// Note that false return immediately leads to the client being removed, so if there's any pending output, we should return true
bool serve_client(int client_sock, Client &client, Cache &cache, std::optional<AOF> &aof){
    if(client.remove_after_send){ // generally indicates a connection that should be closed as soon as we can send its output
        client.input_buffer.clear();
        return true;
    }
    char buf[16384]; // TODO try changing size (now at 16384) for perf - shouldn't need to change as much now
    ssize_t bytes_received = 0;

    while(true){
        bytes_received = recv(client_sock, buf, sizeof(buf), 0); // receive BUFFER_SIZE bytes at once
        if(bytes_received > 0){
            client.input_buffer.append(buf, bytes_received);
        } else if(bytes_received == 0){
            // 0 bytes received so connection should be closed after the output bytes are sent out
            client.remove_after_send = true;
            break;
        } else {
            if(errno == EAGAIN){
                // connection isn't faulty in any way, we just can't receive the bytes currently.
                break;
            }
            perror("recv");
            return false; // need to exit without processing any of the commands already sent
        }
    }

    while(!client.input_buffer.empty()){
        // parse the bytes we just received in the buffer
        resp::ParseResult parse_result = resp::parse(client.input_buffer);
        if(parse_result.status == resp::Status::Error){
            client.output_buffer.append(resp::encode_error("ERR Protocol error: invalid bulk length"));
            client.remove_after_send = true;
            client.input_buffer.clear();
            return true;
        } else if(parse_result.status == resp::Status::Incomplete){
            break; // need to receive more bytes
        } else {
            std::string dispatch_result = (aof == std::nullopt) ?
            Cachelet::dispatch(cache, parse_result.command) :
            Cachelet::dispatch(cache, *aof, parse_result.command);
            // add to this client's "pending" output
            client.output_buffer.append(dispatch_result);
            client.input_buffer.erase(0, parse_result.bytes_consumed);
        }
    }

    return true;

}

bool set_nonblocking(int fd){
    int flags = fcntl(fd, F_GETFL, 0);
    if(flags == -1){
        perror("getfl");
        return false;
    }
    flags |= O_NONBLOCK; // preserve other potentially critical flags

    if(fcntl(fd, F_SETFL, flags) == -1){
        perror("setfl");
        return false;
    } else {
        return true;
    }
}

void remove_client(int epollfd, int fd, std::unordered_map<int, Client> &fd_to_client, std::unordered_set<int> &pending_fds){
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, nullptr);
    close(fd);
    fd_to_client.erase(fd);
    pending_fds.erase(fd);
}

} // namespace

int main(int argc, char* argv[]){
    // parse aof flags, including whether the feature is enabled, the fsync policy, and the filename to write to
    bool aof = false;
    std::string aof_policy_string = "everysec";
    std::optional<Cachelet::AOF::Policy> aof_policy = Cachelet::AOF::Policy::everysec;
    std::string aof_filename = "cachelet.aof";
    for(int i = 1; i < argc; i++){
        std::string flag = argv[i];
        if(flag == "--appendonly"){
            if(i + 1 >= argc){
                std::cerr << "--appendonly needs a value! (yes/no)\n";
                return 1;
            }
            std::string flag_value = argv[++i];
            to_lower_string(flag_value);
            if(flag_value == "yes"){
                aof = true;
            } else if(flag_value == "no"){
                aof = false; // we need this additional guard in case someone writes --appendonly yes --appendonly no (latter overrides)
            } else {
                std::cerr << "--appendonly must be either yes or no\n";
                return 1;
            }
        } else if(flag == "--appendfsync"){
            if(i + 1 >= argc){
                std::cerr << "--appendfsync needs a value! (always/everysec/no)\n"; // TODO if ever add more policies, upd here
                return 1;
            }
            aof_policy_string = argv[++i];
            aof_policy = Cachelet::parse_policy(aof_policy_string);
            if(aof_policy == std::nullopt){
                std::cerr << "--appendfsync invalid value, should be always/everysec/no\n";
                return 1;
            }
        } else if(flag == "--appendfilename"){
            if(i + 1 >= argc){
                std::cerr << "--appendfilename needs a value!\n";
                return 1;
            }
            aof_filename = argv[++i];
        } else {
            std::cerr << "flag not recognized!\n";
            return 1;
        }
    }

    Cache cache; // TODO No way to input max items

    int PORT = 6379;
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if(sock == -1){
        perror("socket");
        return 1;
    }
    int yes_option = 1;
    if(setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &yes_option, sizeof(yes_option)) == -1){
        perror("setsockopt");
        close(sock);
        return 1;
    }

    struct sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if(bind(sock, (struct sockaddr*)&address, sizeof(address)) == -1){
        perror("bind");
        close(sock);
        return 1;
    }

    std::optional<Cachelet::AOF> aof_log;
    if(aof){
        try {
            Cachelet::replay(aof_filename, cache);
            aof_log.emplace(aof_filename, *aof_policy);
        } catch(const std::runtime_error &e){
            std::cerr << e.what() << "\n";
            return 1;
        }
    }

    if(listen(sock, SOMAXCONN) == -1){
        perror("listen");
        close(sock);
        return 1;
    }
    if(!set_nonblocking(sock)){
        close(sock);
        return 1;
    }

    std::cout << "Server listening on port " << PORT << std::endl;

    epoll_event ev{};
    int epollfd = epoll_create1(0);
    if(epollfd == -1){
        perror("epoll_create1");
        return 1;
    }
    ev.events = EPOLLIN;
    ev.data.fd = sock;
    if(epoll_ctl(epollfd, EPOLL_CTL_ADD, sock, &ev) == -1){
        perror("epoll_ctl_add");
        return 1;
    }

    std::unordered_map<int, Client> fd_to_client;
    std::vector<epoll_event> events(64); // scratch vector for the kernel to write to, it receives information about ready file descriptors
    std::unordered_set<int> pending_fds; // clients that might have output, ensure we check after flushing to aof
    // need the above set because we no longer immediately send output whenever can, as we must go through the flush step first, THEN output
    while(true){
        int event_count = (epoll_wait(epollfd, events.data(), events.size(), 
        100)); // change from -1 -> 100ms due to the "every second" aof policy, which requires us to periodically run flush() at least every 1s
        if(event_count == -1){
            perror("epoll_wait");
            return 1;
        }

        for(int i = 0; i < event_count; i++){
            int fd = events[i].data.fd;
            uint32_t flags = events[i].events;
            
            if(fd == sock){ // listener ready
                int new_client_sock = accept(sock, nullptr, nullptr); // don't care about the client ip info or port
                if(new_client_sock < 0){
                    perror("accept");
                    continue;
                }
                if(!set_nonblocking(new_client_sock)){
                    close(new_client_sock);
                    continue;
                }

                epoll_event client_ev{};
                client_ev.events = EPOLLIN;
                client_ev.data.fd = new_client_sock;
                if(epoll_ctl(epollfd, EPOLL_CTL_ADD, new_client_sock, &client_ev) == -1){
                    perror("epoll_ctl_add");
                    close(new_client_sock);
                    continue;
                }
                fd_to_client[new_client_sock] = {};
            } else {
                // client ready
                Client &client = fd_to_client[fd];

                // only read if there might be readable information from the socket
                // this includes EPOLLHUP -> a connection hung up, and we handle the logic for that case within this block
                // also includes EPOLLERR -> error handling is also done within this block
                if(flags & (EPOLLIN | EPOLLHUP | EPOLLERR)){
                    if(!serve_client(fd, client, cache, aof_log)){
                        // false return = connection closed, remove the fd
                        remove_client(epollfd, fd, fd_to_client, pending_fds);
                        continue;
                    }
                    pending_fds.insert(fd);
                }

            }
            if(flags & EPOLLOUT){
                pending_fds.insert(fd);             
            }
        }
        if(aof_log != std::nullopt){
            if(!(aof_log->Cachelet::AOF::flush())){
                std::cerr << "aof write failed\n";
                return 1;
            }
        }
        std::unordered_set<int> failed_fds;
        for(int fd : pending_fds){
            if(fd_to_client.find(fd) != fd_to_client.end()){
                if(!handle_send(epollfd, fd, fd_to_client[fd])){
                    failed_fds.insert(fd);
                }
                // if the conenction has no more input to offer us, AND we've sent everything we need to, it's safe to close
                if(fd_to_client[fd].remove_after_send && fd_to_client[fd].output_buffer.empty()){
                    failed_fds.insert(fd);
                }
            } 
        }
        for(int failed_fd : failed_fds){ // These are clients with active failures, so we must remove the connections entirely
            remove_client(epollfd, failed_fd, fd_to_client, pending_fds);
        }
        pending_fds.clear(); // we've dealt with the possible outputs from each of the candidate fds, so clear until next iteration
    }
}
