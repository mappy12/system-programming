#include "common.h"
#include <string>
#include <iostream>
#include <cstring>
#include <netdb.h>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: ./client <server_address> [port]" << std::endl;
        return 1;
    }

    std::string host = argv[1];
    unsigned short port = SERVER_PORT;
    if (argc > 2) {
        port = std::stoi(argv[2]);
    }

    int sock_fd = check(make_socket(SOCKET_TYPE));

    sockaddr_in dest_address{};
    dest_address.sin_family = AF_INET;
    dest_address.sin_port = htons(port);

    if (inet_pton(AF_INET, host.c_str(), &dest_address.sin_addr) <= 0) {
        struct hostent* he = gethostbyname(host.c_str());
        if (he == NULL) {
            herror("gethostbyname");
            close(sock_fd);
            return 1;
        }
        memcpy(&dest_address.sin_addr, he->h_addr_list[0], he->h_length);
    }

    std::cout << "Connecting to " << dest_address << "..." << std::endl;
    check(connect(sock_fd, (sockaddr*)&dest_address, sizeof(dest_address)));
    std::cout << "Connected. Enter numbers (1-100) to guess. Type 'q' to quit." << std::endl;

    char recv_buffer[64];
    std::string message;

    while (true) {
        std::cout << "> ";
        if (!std::getline(std::cin, message)) {
            break;
        }

        if (message == "q" || message == "exit") {
            break;
        }

        std::string send_msg = message + "\n";
        if (send(sock_fd, send_msg.c_str(), send_msg.size(), 0) == -1) {
            perror("send");
            break;
        }

        int size = recv(sock_fd, recv_buffer, sizeof(recv_buffer) - 1, 0);
        
        if (size <= 0) {
            if (size == 0) {
                std::cout << "Server disconnected gracefully." << std::endl;
            } else {
                perror("recv");
            }
            break;
        }
        
        recv_buffer[size] = '\0';

        std::cout << "Server: " << recv_buffer;

        if (std::string(recv_buffer).find("Correct") != std::string::npos) {
            std::cout << "Game Over!" << std::endl;
            break;
        }
    }

    shutdown(sock_fd, SHUT_RDWR);
    close(sock_fd);

    return 0;
}