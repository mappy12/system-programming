#include "common.h"
#include <signal.h>
#include <string>
#include <iostream>
#include <cstring>

int main(int argc, char* argv[]) {

    ignore_sigpipe();

    if (argc < 2) {
        std::cerr << "Usage: ./client <server_ip> [port]" << std::endl;
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

    if (inet_pton(AF_INET,
                  host.c_str(),
                  &dest_address.sin_addr) <= 0) {

        std::cerr << "Invalid IP address: " << host << std::endl;

        close(sock_fd);

        return 1;
    }

    std::cout << "Connecting to "
              << dest_address
              << "..."
              << std::endl;

    check(connect(sock_fd,
                  (sockaddr*)&dest_address,
                  sizeof(dest_address)));

    std::cout << "Connected."
              << std::endl;

    std::string input;

    while (true) {

        std::cout << "> ";

        if (!std::getline(std::cin, input)) {
            break;
        }

        if (input == "q" || input == "exit") {
            break;
        }

        int32_t guess;

        try {
            guess = std::stoi(input);
        }
        catch (...) {
            std::cout << "Invalid number." << std::endl;
            continue;
        }

        int32_t network_guess = htonl(guess);

        int sent = send(sock_fd,
                        &network_guess,
                        sizeof(network_guess),
                        0);

        if (sent == -1) {
            perror("send");
            break;
        }

        int32_t response;

        int size = recv(sock_fd,
                        &response,
                        sizeof(response),
                        MSG_WAITALL);

        if (size <= 0) {

            if (size == 0) {
                std::cout << "Server disconnected."
                          << std::endl;
            }
            else {
                perror("recv");
            }

            break;
        }

        response = ntohl(response);

        switch (response) {

            case LOWER:
                std::cout << "Server: Lower" << std::endl;
                break;

            case HIGHER:
                std::cout << "Server: Higher" << std::endl;
                break;

            case CORRECT:
                std::cout << "Server: Correct! You won!"
                          << std::endl;

                shutdown(sock_fd, SHUT_RDWR);
                close(sock_fd);

                return 0;

            default:
                std::cout << "Server: Unknown response"
                          << std::endl;
        }
    }

    shutdown(sock_fd, SHUT_RDWR);

    close(sock_fd);

    return 0;
}