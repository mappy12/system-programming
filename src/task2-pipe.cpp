#include "check.hpp"
#include <iostream>
#include <cstdlib>
#include <ctime>
#include <cerrno>
#include <unistd.h>
#include <sys/wait.h>
#include <csignal>

using namespace std;

volatile sig_atomic_t child_dead = 0;

void sigchld_handler(int) {
    child_dead = 1;
}

bool is_alive(pid_t pid) {
    if (pid <= 0) return false;
    if (kill(pid, 0) == 0) return true;
    if (errno == EPERM) return true;
    return false;
}

bool write_int(int fd, int value, pid_t peer) {
    while (true) {
        if (!is_alive(peer)) return false;
        ssize_t n = write(fd, &value, sizeof(value));
        if (n == sizeof(value)) return true;
        if (n == -1 && errno == EINTR) continue;
        if (n == -1 && errno == EPIPE) return false;
        check(n);
    }
}

bool read_int(int fd, int &value, pid_t peer) {
    while (true) {
        if (!is_alive(peer)) return false;
        ssize_t n = read(fd, &value, sizeof(value));
        if (n == sizeof(value)) return true;
        if (n == 0) return false;
        if (n == -1 && errno == EINTR) continue;
        check(n);
    }
}

void riddler(int secret, int read_fd, int write_fd, pid_t peer, int round) {
    cout << "Раунд " << round << ". PID " << getpid() << " загадал число " << secret << endl;
    fflush(stdout);
    while (is_alive(peer)) {
        int guess;
        if (!read_int(read_fd, guess, peer)) break;
        cout << "Раунд " << round << ". PID " << getpid() << " получил: " << guess << "\n";
        fflush(stdout);
        int correct = (guess == secret);
        if (!write_int(write_fd, correct, peer)) break;
        if (correct) break;
    }
}

void guesser(int N, int write_fd, int read_fd, pid_t peer, int round) {
    int attempts = 0;
    while (is_alive(peer)) {
        int guess = rand() % N + 1;
        attempts++;
        cout << "Раунд " << round << ". PID " << getpid() << " отправил: " << guess << "\n";
        fflush(stdout);
        if (!write_int(write_fd, guess, peer)) break;
        int response;
        if (!read_int(read_fd, response, peer)) break;
        if (response) {
            cout << "Раунд " << round << ". PID " << getpid()
                 << " угадал число " << guess
                 << " за " << attempts << " попыток.\n";
            fflush(stdout);
            break;
        }
    }
}

void play_game(bool is_parent, int read_fd, int write_fd, int N, int rounds, pid_t peer) {
    for (int r = 1; r <= rounds && is_alive(peer); ++r) {
        bool i_am_riddler = (r % 2 == (is_parent ? 1 : 0));
        int secret = rand() % N + 1;
        if (i_am_riddler)
            riddler(secret, read_fd, write_fd, peer, r);
        else
            guesser(N, write_fd, read_fd, peer, r);
    }
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        cerr << "Usage: " << argv[0] << " N rounds\n";
        return 1;
    }

    int N = atoi(argv[1]);
    int rounds = atoi(argv[2]);
    srand(time(nullptr) ^ getpid());
    signal(SIGPIPE, SIG_IGN);

    int pipe_pc[2]; check(pipe(pipe_pc));
    int pipe_cp[2]; check(pipe(pipe_cp));

    struct sigaction sa{};
    sa.sa_handler = sigchld_handler;
    sigaction(SIGCHLD, &sa, nullptr);

    pid_t pid = check(fork());
    if (pid == 0) {
        pid_t parent_id = getppid();
        check(close(pipe_pc[1]));
        check(close(pipe_cp[0]));
        int read_fd = pipe_pc[0];
        int write_fd = pipe_cp[1];
        play_game(false, read_fd, write_fd, N, rounds, parent_id);
        check(close(read_fd));
        check(close(write_fd));
        cout << "Ребёнок завершен\n";
        return 0;
    }

    check(close(pipe_pc[0]));
    check(close(pipe_cp[1]));
    int read_fd = pipe_cp[0];
    int write_fd = pipe_pc[1];
    play_game(true, read_fd, write_fd, N, rounds, pid);
    check(close(read_fd));
    check(close(write_fd));
    waitpid(pid, nullptr, 0);
    cout << "Родитель завершен\n";
    return 0;
}
