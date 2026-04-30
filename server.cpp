#include "common.h"
#include <sys/wait.h>
#include <signal.h>
#include <cstdlib>
#include <ctime>
#include <string>
#include <sstream>
#include <pthread.h>
#include <sys/mman.h>

pthread_mutex_t* log_mutex = nullptr;

void log(const sockaddr_in& addr, const std::string& msg) {
    pthread_mutex_lock(log_mutex);
    std::cout << addr << ": " << msg << std::endl;
    pthread_mutex_unlock(log_mutex);
}

void sigchld_handler(int s) {
    while(waitpid(-1, NULL, WNOHANG) > 0);
}

void handle_game(int conn_fd, sockaddr_in client_addr) {
    srand(time(NULL) ^ getpid());
    int target = rand() % 100 + 1;
    
    char buffer[64];
    bool game_active = true;

    log(client_addr, "Game started. Guess number 1-100.");

    while (game_active) {
        int size = recv(conn_fd, buffer, sizeof(buffer) - 1, 0);
        
        if (size <= 0) {
            log(client_addr, "Disconnected (recv error or close).");
            break;
        }
        
        buffer[size] = '\0';
        std::string guess_str(buffer);

        while (!guess_str.empty() && (guess_str.back() == '\n' || guess_str.back() == '\r')) {
            guess_str.pop_back();
        }

        if (guess_str.empty()) continue;

        try {
            int guess = std::stoi(guess_str);
            std::string response;

            if (guess == target) {
                response = "Correct! You won!\n";
                log(client_addr, "Guessed correctly: " + guess_str);
                game_active = false;
            } else if (guess < target) {
                response = "Higher\n";
                log(client_addr, "Guess: " + guess_str + " -> Higher");
            } else {
                response = "Lower\n";
                log(client_addr, "Guess: " + guess_str + " -> Lower");
            }

            send(conn_fd, response.c_str(), response.size(), 0);

        } catch (...) {
            std::string err = "Invalid input. Send a number.\n";
            send(conn_fd, err.c_str(), err.size(), 0);
            log(client_addr, "Invalid input: " + guess_str);
        }
    }
    
    close(conn_fd);
    exit(0);
}

int main(int argc, char* argv[]) {
    unsigned short port = SERVER_PORT;
    if (argc > 1) {
        port = std::stoi(argv[1]);
    }

    log_mutex = static_cast<pthread_mutex_t*>(mmap(NULL, sizeof(pthread_mutex_t),
                                                   PROT_READ | PROT_WRITE,
                                                   MAP_SHARED | MAP_ANONYMOUS, -1, 0));

    if (log_mutex == MAP_FAILED) {
        perror("mmap failed");
        return 1;
    }
    
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);

    pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(log_mutex, &attr);
    pthread_mutexattr_destroy(&attr);

    struct sigaction sa;
    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction failed");
        return 1;
    }

    auto server_address = local_addr(port); 
    
    int listening_socket = check(make_socket(SOCKET_TYPE));
    check(bind(listening_socket, (sockaddr*)&server_address, sizeof(server_address)));
    check(listen(listening_socket, 10)); // Backlog 10

    std::cout << "Server started on port " << port << " (PID: " << getpid() << ")" << std::endl;

    while (true) {
        sockaddr_in connected_address{};
        socklen_t addrlen = sizeof(connected_address);

        int connected_socket = accept(listening_socket, (sockaddr*)&connected_address, &addrlen);
        
        if (connected_socket == -1) {
            if (errno == EINTR) continue;
            perror("accept");
            continue;
        }

        pid_t pid = fork();
        
        if (pid < 0) {
            perror("fork");
            close(connected_socket);
        } 
        else if (pid == 0) {
            close(listening_socket);
            handle_game(connected_socket, connected_address);
        } 
        else {
            close(connected_socket);
        }
    }

    close(listening_socket);
    pthread_mutex_destroy(log_mutex);
    munmap(log_mutex, sizeof(pthread_mutex_t));
    return 0;
}