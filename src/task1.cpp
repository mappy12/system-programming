#include <csignal>
#include <cstdlib>
#include <unistd.h>
#include <vector>
#include <algorithm>
#include <random>b

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

bool run_riddler(pid_t peer_pid, int upper_bound, int round, const sigset_t& wait_mask) {
    int secret = rand() % upper_bound + 1;
    printf("\n[Загадывающий PID=%d] Раунд %d/%d: загадал %d (1...%d)\n",
        getpid(), round, ROUNDS, secret, upper_bound);

    while (g_last_sig != SIG_READY) {
        if (!wait_signal(wait_mask)) return false;
    }

    g_last_sig = 0;

    int attempts = 0;

    while (true) {
        if (!wait_signal(wait_mask)) return false;
        if (g_last_sig != SIG_GUESS) continue;

        int guess = static_cast<int>(g_sig_value);
        ++attempts;
        printf("[Загадывающий PID=%d] Получено: %d\n", getpid(), guess);

        if (guess == secret) {
            check(kill(peer_pid, SIG_HIT));
            printf("[Загадывающий PID=%d] Верно! Угадано за %d попыток.\n", getpid(), attempts);
            return true;
        } else {
            check(kill(peer_pid, SIG_MISS));
        }
    }
}

static bool run_guesser(pid_t peer_pid, int upper_bound, int round, const sigset_t& wait_mask) {
    printf("\n[Угадывающий  PID=%d] === Раунд %d/%d: угадываю (1..%d) ===\n",
           getpid(), round, ROUNDS, upper_bound);

    std::vector<int> numbers;
    for (int i = 1; i <= upper_bound; i++)
        numbers.push_back(i);
    std::shuffle(numbers.begin(), numbers.end(),
                 std::default_random_engine(
                     static_cast<unsigned>(time(nullptr)) ^ static_cast<unsigned>(getpid())
                 ));

    send_rt(peer_pid, SIG_READY, 0);

    int attempts = 0;
    int index = 0;

    while (true) {
        int guess = numbers[index++];
        ++attempts;

        printf("[Угадывающий  PID=%d] Попытка %d: предполагаю %d\n",
               getpid(), attempts, guess);

        send_rt(peer_pid, SIG_GUESS, guess);

        if (!wait_signal(wait_mask)) return false;

        if (g_last_sig == SIG_HIT) {
            printf("[Угадывающий  PID=%d] Угадал %d за %d попыток!\n",
                   getpid(), guess, attempts);
            return true;
        }
    }
}

static void run_game(pid_t peer_pid, int upper_bound, bool is_parent) {

    sigset_t block_all;
    sigfillset(&block_all);
    check(sigprocmask(SIG_SETMASK, &block_all, nullptr));

    set_handler_rt(SIG_GUESS, handler_rt);
    set_handler_rt(SIG_READY, handler_rt);
    set_handler_plain(SIG_HIT,  handler_plain);
    set_handler_plain(SIG_MISS, handler_plain);
    set_handler_plain(SIGCHLD,  handler_plain);
    set_handler_plain(SIGTERM,  handler_plain);

    sigset_t wait_mask;
    sigfillset(&wait_mask);
    sigdelset(&wait_mask, SIG_GUESS);
    sigdelset(&wait_mask, SIG_READY);
    sigdelset(&wait_mask, SIG_HIT);
    sigdelset(&wait_mask, SIG_MISS);
    sigdelset(&wait_mask, SIGCHLD);
    sigdelset(&wait_mask, SIGTERM);
 
    srand(static_cast<unsigned>(time(nullptr)) ^ static_cast<unsigned>(getpid()));

    for (int round = 1; round <= ROUNDS; ++round) {

        bool i_am_riddler = is_parent ? (round % 2 == 1) : (round % 2 == 0);
 
        bool ok;
        if (i_am_riddler) {
            ok = run_riddler(peer_pid, upper_bound, round, wait_mask);
        } else {
            ok = run_guesser(peer_pid, upper_bound, round, wait_mask);
        }
 
        if (!ok) {
            printf("[PID=%d] Второй игрок завершился в раунде %d, выхожу.\n",
                   getpid(), round);
            return;
        }
    }
 
    printf("\n[PID=%d] Все раунды сыграны!\n", getpid());
}


int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Использование: %s <N>\n", argv[0]);
        fprintf(stderr, " N - верхняя граница числа (>= 2)\n");
        return EXIT_FAILURE;
    }

    int upper_bound = atoi(argv[1]);
    if (upper_bound < 2) {
        fprintf(stderr, "N должен быть >= 2\n");
        return EXIT_FAILURE;
    }

    printf("Игра 'Угадай число'. Диапазон: 1..%d\n\n", upper_bound, ROUNDS);

    pid_t child_pid = check(fork());

    if (child_pid > 0) {
        run_game(child_pid, upper_bound, true);

        int stat;

        check(waitpid(child_pid, &stat, 0));
        printf("\n[Родитель] Игра окончена!\n");
    } else {
        run_game(getpid(), upper_bound, false);
        exit(EXIT_SUCCESS);
    }

    return EXIT_SUCCESS;
}

