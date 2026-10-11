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

int main(){
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    int PORT = 6379;
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

    struct sockaddr_in address;
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
    int client_sock = accept(sock, nullptr, nullptr);
    if(client_sock < 0){
        perror("accept");
        close(sock);
        return -1;
    }

    std::cout << "Client connected!" << std::endl;

    char buf[1024];
    std::string full;
    ssize_t bytes_received = 0;

    while(true){
        bytes_received = recv(client_sock, buf, sizeof(buf), 0);
        if(bytes_received > 0){
            full += buf;
        } else if(bytes_received == 0){
            // connection closed
            break;
        } else {
            perror("recv");
            break;
        }
    }
    // std::cout << "test: " << full + "\r\n" << std::endl;

    if(send(client_sock, "+PONG\r\n", 7, 0) == -1){
        perror("send");
        close(sock);
        return -1;
    }

    close(sock);
    close(client_sock);
    return 0;

}