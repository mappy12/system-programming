#define _POSIX_C_SOURCE 200809L

#include <csignal>

#include "check.hpp"

#define SIG_GUESS SIGRTMIN
#define SIG_MISS (SIGRTMIN + 1)
#define SIG_READY (SIGRTMIN + 2)
#define SIG_HIT SIGUSR1

const int ROUNDS = 10;

static volatile sig_atomic_t g_last_sig = 0;
static volatile sig_atomic_t g_sig_value = 0;
static volatile sig_atomic_t g_peer_dead = 0;

void handler_plain(int signum) {
    g_last_sig = signum;
    if (signum == SIGCHLD || signum == SIGTERM)
        g_peer_dead = 1;
}

void handler_rt(int signum, siginfo_t* si) {
    g_last_sig = signum;
    g_sig_value = si->si_value.sival_int;
}

void set_handler_plain(int sig, void (*fn)(int)) {
    struct sigaction sa {};
    sa.sa_handler = fn;
    sigemptyset(&sa.sa_mask);
    check(sigaction(sig, &sa, nullptr));
}

void set_handler_rt(int sig, void (*fn)(int, siginfo_t*, void*)) {
    struct sigaction sa {};
    sa.sa_sigaction = fn;
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    check(sigaction(sig, &sa, nullptr));
}

void send_rt(pid_t pid, int sig, int value) {
    union sigval sv;
    sv.sival_int = value;
    check(sigqueue(pid, sig, sv));
}

int main() {
    return 0;
}

