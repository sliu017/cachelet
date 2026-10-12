#include "cachelet/cache.hpp"
#include "cachelet/dispatch.hpp"
#include "cachelet/resp.hpp"

#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

#include <cstdio>
#include <cerrno>
#include <cstring>

#include <iostream>
#include <string>

using namespace Cachelet;

namespace {

// need a safe way to send() in case it doesn't receive all bytes requested to be sent
// therefore, whenever we send anything to the client, we must use this method
bool send_all(int client_sock, std::string dispatch_result){
    while(!dispatch_result.empty()){
        ssize_t send_result = send(client_sock, dispatch_result.data(), dispatch_result.size(), 0);
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

void serve_client(int client_sock, Cache &cache){
    char buf[1024];
    std::string buffer;
    ssize_t bytes_received = 0;

    while(true){
        bytes_received = recv(client_sock, buf, sizeof(buf), 0);
        if(bytes_received > 0){
            buffer.append(buf, bytes_received);
        } else if(bytes_received == 0){
            return; // connection closed
        } else {
            perror("recv");
            return;
        }

        while(!buffer.empty()){
            // parse the bytes we just received in the buffer
            resp::ParseResult parse_result = resp::parse(buffer);
            if(parse_result.status == resp::Status::Error){
                send_all(client_sock, resp::encode_error("ERR Protocol error: invalid bulk length"));
                return;
            } else if(parse_result.status == resp::Status::Incomplete){
                break; // need to receive more bytes
            } else {
                std::string dispatch_result = Cachelet::dispatch(cache, parse_result.command);
                // try to send the response to the client
                if(!send_all(client_sock, dispatch_result)){
                    return;
                }
                // move the buffer forward after parsing this command's bytes
                buffer = buffer.substr(parse_result.bytes_consumed);
            }
        }
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

    std::cout << "Server listening on port " << PORT << std::endl;

    while(true){
        int client_sock = accept(sock, nullptr, nullptr);
        if(client_sock < 0){
            perror("accept");
            continue;
        }

        std::cout << "Client connected!" << std::endl;


        serve_client(client_sock, cache);

        close(client_sock);
    }
    // Unreachable in theory because the only way to terminate the server, currently, is to ctrl c interrupt. 
    close(sock);
    return 0;

}