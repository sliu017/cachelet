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
    std::string buffer;
};

namespace {

    


// need a safe way to send() in case it doesn't receive all bytes requested to be sent
// therefore, whenever we send anything to the client, we must use this method
bool send_all(int client_sock, std::string dispatch_result){
    while(!dispatch_result.empty()){
        ssize_t send_result = send(client_sock, dispatch_result.data(), dispatch_result.size(), MSG_NOSIGNAL);
        if(send_result == -1){
            perror("send");
            return false;
        } else if(send_result < dispatch_result.size()){
            dispatch_result = dispatch_result.substr(send_result);
        } else {
            break;
        }
    }
    return true;
}

bool serve_client(int client_sock, Client &client, Cache &cache){ // returns true if the connection is still open
    char buf[1024];
    ssize_t bytes_received = 0;

    bytes_received = recv(client_sock, buf, sizeof(buf), 0);
    if(bytes_received > 0){
        client.buffer.append(buf, bytes_received);
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

    while(!client.buffer.empty()){
        // parse the bytes we just received in the buffer
        resp::ParseResult parse_result = resp::parse(client.buffer);
        if(parse_result.status == resp::Status::Error){
            send_all(client_sock, resp::encode_error("ERR Protocol error: invalid bulk length"));
            return false;
        } else if(parse_result.status == resp::Status::Incomplete){
            break; // need to receive more bytes
        } else {
            std::string dispatch_result = Cachelet::dispatch(cache, parse_result.command);
            // try to send the response to the client
            if(!send_all(client_sock, dispatch_result)){
                return false;
            }
            // move the buffer forward after parsing this command's bytes
            client.buffer = client.buffer.substr(parse_result.bytes_consumed);
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

    if(listen(sock, 10) == -1){
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
                    int client_sock = accept(sock, nullptr, nullptr); // don't care about the client ip info or port
                    if(client_sock < 0){
                        perror("accept");
                        continue;
                    }
                    if(!set_nonblocking(client_sock)){
                        close(client_sock);
                        continue;
                    }
                    to_add.push_back({client_sock, POLLIN, 0});
                    fd_to_client[client_sock] = {};
                } else {
                    // client is ready
                    if(!serve_client(pfds[i].fd, fd_to_client[pfds[i].fd], cache)){
                        // false return = connection closed, remove the fd
                        to_remove.push_back(pfds[i].fd);
                    }
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

    // while(true){
    //     int client_sock = accept(sock, nullptr, nullptr);
    //     if(client_sock < 0){
    //         perror("accept");
    //         continue;
    //     }

    //     std::cout << "Client connected!" << std::endl;


    //     pollfd first_connection = {
    //         .fd = client_sock,
    //         .events = POLLIN | POLLOUT,
    //         .revents = 0
    //     };
    //     pfds.push_back(first_connection);
    //     clients.push_back({});


    //     serve_client(client_sock, cache);

    //     close(client_sock);
    // }
    // // Unreachable in theory because the only way to terminate the server, currently, is to ctrl c interrupt. 
    // close(sock);
    // return 0;

}