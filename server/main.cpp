#include "cachelet/cache.hpp"
#include "cachelet/dispatch.hpp"
#include "cachelet/resp.hpp"

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
#include <algorithm>

#include "poll.h"

using namespace Cachelet;

struct Client {
    std::string input_buffer;
    std::string output_buffer;
};

namespace {



// when we aren't guaranteed to be able to send all the bytes (could have a partial send)
// returns the number of bytes LEFT, except for -1, which indicates a send failure
ssize_t try_send(int client_sock, Client &client){
    ssize_t original_byte_count = client.output_buffer.size();
    while(!client.output_buffer.empty()){
        ssize_t bytes_sent = send(client_sock, client.output_buffer.data(), client.output_buffer.size(), MSG_NOSIGNAL);
        if(bytes_sent > 0){
            original_byte_count -= bytes_sent;
            client.output_buffer.erase(0, bytes_sent);
            continue;
        } else if(bytes_sent == -1){
            if(errno == EAGAIN || errno == EWOULDBLOCK){
            // kernel full, can't send any more. wait for POLLOUT signal
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
// it attempts to send bytes and responds according to the result, including setting the POLLOUT flag
bool handle_send(pollfd &pfd, Client &client){
    int client_sock = pfd.fd;
    ssize_t bytes_left = try_send(client_sock, client); // note try_send returns bytes remaining
    if(bytes_left == -1){ // failure to send bytes
        return false;
    } else if(bytes_left > 0){
        pfd.events |= POLLOUT;
    } else { // all bytes sent! e.g. no bytes remaining
        // remove POLLOUT flag since we don't have "pending" sends anymore
        if((pfd.events & POLLOUT) == POLLOUT){
            pfd.events ^= POLLOUT;
        }
    }
    return true;
}

bool serve_client(int client_sock, Client &client, Cache &cache){ // returns true if the connection is still open
    char buf[1024]; // TODO try changing size for perf.
    ssize_t bytes_received = 0;

    bytes_received = recv(client_sock, buf, sizeof(buf), 0); // receive 1024 bytes at once
    if(bytes_received > 0){
        client.input_buffer.append(buf, bytes_received);
    } else if(bytes_received == 0){
        return false; // connection closed
    } else {
        if(errno == EAGAIN){
            // connection isn't faulty in any way, we just can't receive the bytes currently.
            return true;
        }
        perror("recv");
        return false;
    }

    while(!client.input_buffer.empty()){
    // parse the bytes we just received in the buffer
    resp::ParseResult parse_result = resp::parse(client.input_buffer);
        if(parse_result.status == resp::Status::Error){
            client.output_buffer.append(resp::encode_error("ERR Protocol error: invalid bulk length"));
            try_send(client_sock, client);
            return false;
        } else if(parse_result.status == resp::Status::Incomplete){
            break; // need to receive more bytes
        } else {
            std::string dispatch_result = Cachelet::dispatch(cache, parse_result.command);
            // add to this client's "pending" output
            client.output_buffer.append(dispatch_result);
            client.input_buffer.erase(0, parse_result.bytes_consumed);

            // let main handle sending the bytes because we might need to edit a pfd flag depending on the result
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

} // namespace

int main(){
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    int PORT = 6379;
    Cache cache; // TODO No way to input max items

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

    std::vector<pollfd> pfds;
    pfds.push_back({sock, POLLIN, 0});
    std::unordered_map<int, Client> fd_to_client;

    while(true){
        if(poll(pfds.data(), pfds.size(), -1) == -1){ // don't periodically check, only act when alerted to requests from client
            perror("poll");
            break;
        }
        std::vector<pollfd> to_add;
        std::vector<int> to_remove; // avoid concurrent mutation
        for(int i = 0; i < pfds.size(); i++){
            if(pfds[i].revents & POLLIN){
                if(pfds[i].fd == sock){
                    // listener is ready, i.e. new client available
                    int new_client_sock = accept(sock, nullptr, nullptr); // don't care about the client ip info or port
                    if(new_client_sock < 0){
                        perror("accept");
                        continue;
                    }
                    if(!set_nonblocking(new_client_sock)){
                        close(new_client_sock);
                        continue;
                    }
                    to_add.push_back({new_client_sock, POLLIN, 0});
                    fd_to_client[new_client_sock] = {};
                } else {
                    // client is ready
                    int client_sock = pfds[i].fd;
                    Client &client = fd_to_client[pfds[i].fd];

                    if(!serve_client(client_sock, client, cache)){
                        // false return = connection closed, remove the fd
                        to_remove.push_back(pfds[i].fd);
                        continue;
                    }
                    
                    if(!handle_send(pfds[i], client)){
                        to_remove.push_back(pfds[i].fd);
                        continue;
                    }

                }
            }
            if(pfds[i].revents & POLLOUT){
                if(!handle_send(pfds[i], fd_to_client[pfds[i].fd])){
                    to_remove.push_back(pfds[i].fd);
                    continue;
                }
            }
        }
        pfds.insert(pfds.end(), to_add.begin(), to_add.end());
        std::erase_if(pfds, [&to_remove](const pollfd &x){
            return (std::find(to_remove.begin(), to_remove.end(), x.fd) != to_remove.end());
        });
        for(int i = 0; i < to_remove.size(); i++){
            close(to_remove[i]);
            fd_to_client.erase(to_remove[i]);
        }
    }

}