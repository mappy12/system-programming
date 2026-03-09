#include <csignal>
#include <cstdlib>
#include <unistd.h>

#include "check.hpp"

#define SIG_GUESS SIGRTMIN
#define SIG_READY (SIGRTMIN + 1)
#define SIG_HIT SIGUSR1
#define SIG_MISS SIGUSR2

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

bool wait_signal(const sigset_t& wait_mask) {
    check_except(sigsuspend(&wait_mask), EINTR);
    return g_peer_dead == 0;
}

void run_riddler(pid_t guesser_pid, int upper_bound) {
    sigset_t block_all;
    sigfillset(&block_all);
    check(sigprocmask(SIG_SETMASK, &block_all, nullptr));

    set_handler_rt(SIG_GUESS, handler_rt);
    set_handler_rt(SIG_READY, handler_rt);
    set_handler_plain(SIGCHLD, handler_plain);
    set_handler_plain(SIGTERM, handler_plain);

    sigset_t wait_mask;
    sigfillset(&wait_mask);
    sigdelset(&wait_mask, SIG_GUESS);
    sigdelset(&wait_mask, SIG_READY);
    sigdelset(&wait_mask, SIGCHLD);
    sigdelset(&wait_mask, SIGTERM);

    srand(static_cast<unsigned>(time(nullptr)) ^ static_cast<unsigned>(getpid()));

    printf("[Загадывающий (PID = %d)] Ожидание готовности угадывающего...\n", getpid());

    while (g_last_sig != SIG_READY) {
        if (!wait_signal(wait_mask)) {
            printf("[Загадывающий] Угадывающий завершился до старта, выход.\n");
            return;
        }
    }

    for (int round = 1; round <= ROUNDS; round++) {
        int secret = rand() % upper_bound + 1;
        printf("\n[Загадывающий] Раунд %d/%d: загадал %d\n", round, ROUNDS, secret);

        int attempts = 0;

        while (true) {
            if (!wait_signal(wait_mask)) {
                printf("[Загадывающий] Угадывающий завершился на раунде %d, выход", round);
                return;
            }

            if (g_last_sig != SIG_GUESS) continue;

            int guess = static_cast<int>(g_sig_value);
            atempts++;

            printf("[Загадывающий] Полученное число: %d\n", guess);

            if (guess == secret) {
                check(kill(guesser_pid, SIG_HIT));
                printf("[Загадывающий] ВЕРНО! Угадано за %d попыток\n", attempts);
                break;
            } else {
                check(kill(guesser_pid, SIG_MISS));
            }
        }
    }

    printf("\n[Загадывающий] Все раунды сыграны, завершение угадывающего\n");
    check(kill(guesser_pid, SIGTERM));
}

int main() {
    return 0;
}

