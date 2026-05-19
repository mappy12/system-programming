#ifndef COMMON_H
#define COMMON_H
#include <signal.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <iostream>
#include <cstdint>
#include "check.hpp"

constexpr unsigned short SERVER_PORT = 60002;
constexpr int SOCKET_TYPE = SOCK_STREAM;

enum Response {
    LOWER = -1,
    CORRECT = 0,
    HIGHER = 1,
};

inline std::ostream& operator<<(std::ostream& s, const sockaddr_in& addr) {
    union {
        in_addr_t x;
        unsigned char c[sizeof(in_addr)];
    } t{};

    t.x = addr.sin_addr.s_addr;

    return s
        << std::to_string(t.c[0]) << "."
        << std::to_string(t.c[1]) << "."
        << std::to_string(t.c[2]) << "."
        << std::to_string(t.c[3]) << ":"
        << std::to_string(ntohs(addr.sin_port));
}

inline int make_socket(int type) {
    switch (type) {
        case SOCK_STREAM:
            return socket(AF_INET, SOCK_STREAM, 0);

        case SOCK_SEQPACKET:
            return check(socket(AF_INET, SOCK_STREAM, IPPROTO_SCTP));

        default:
            errno = EINVAL;
            return -1;
    }
}

inline sockaddr_in local_addr(unsigned short port) {
    sockaddr_in addr{};

    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(port);
    addr.sin_family = AF_INET;

    return addr;
}

inline void ignore_sigpipe() {
    struct sigaction sa{};
    sa.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, &sa, nullptr);
}

#endif