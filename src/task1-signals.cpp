#include <iostream>
#include <csignal>
#include <unistd.h>
#include <sys/wait.h>
#include <cstdlib>
#include <ctime>
#include <vector>

#include "check.hpp"

using namespace std;

#define SIG_GUESS SIGRTMIN
#define SIG_WIN   SIGUSR1
#define SIG_LOSE  SIGUSR2

volatile sig_atomic_t guess_ready = 0;
volatile sig_atomic_t guess_value = 0;

volatile sig_atomic_t result_ready = 0;
volatile sig_atomic_t result = 0;

volatile sig_atomic_t terminate_flag = 0;


void guess_handler(int, siginfo_t *info, void *) {
    guess_value = info->si_value.sival_int;
    guess_ready = 1;
}

void result_handler(int sig) {
    result = (sig == SIG_WIN);
    result_ready = 1;
}

void term_handler(int) {
    terminate_flag = 1;
}

void setup_handlers() {
    struct sigaction sa_guess{};
    sa_guess.sa_flags = SA_SIGINFO;      
    sa_guess.sa_sigaction = guess_handler;
    sigemptyset(&sa_guess.sa_mask);      
    sigaction(SIG_GUESS, &sa_guess, nullptr);

    struct sigaction sa_simple{};
    sa_simple.sa_handler = result_handler; 
    sigemptyset(&sa_simple.sa_mask);
    sa_simple.sa_flags = 0;          

    sigaction(SIG_WIN, &sa_simple, nullptr);
    sigaction(SIG_LOSE, &sa_simple, nullptr);

    struct sigaction sa_term{};
    sa_term.sa_handler = term_handler;
    sigemptyset(&sa_term.sa_mask);
    sa_term.sa_flags = 0;

    sigaction(SIGINT, &sa_term, nullptr);
    sigaction(SIGTERM, &sa_term, nullptr);
}

void wait_for(volatile sig_atomic_t &flag, sigset_t &wait_mask) {
    while (!flag && !terminate_flag) {
        sigsuspend(&wait_mask);
    }
    flag = 0;
}

void send_guess(pid_t pid, int value) {
    union sigval val;
    val.sival_int = value;
    sigqueue(pid, SIG_GUESS, val);
}


int main(int argc, char *argv[]) {
    if (argc < 2) {
        cout << "Usage: " << argv[0] << " N\n";
        return 1;
    }

    int N = atoi(argv[1]);
    
    sigset_t mask_game, old_mask;
    sigemptyset(&mask_game);
    sigaddset(&mask_game, SIGRTMIN);
    sigaddset(&mask_game, SIG_WIN); 
    sigaddset(&mask_game, SIG_LOSE); 

    check(sigprocmask(SIG_SETMASK, &mask_game, &old_mask));

    setup_handlers();

    pid_t child = fork();

    if (child < 0) {
        perror("fork");
        return 1;
    }

    srand(time(nullptr) ^ getpid());

    bool is_parent = (child != 0);
    const char* role = is_parent ? "Parent" : "Child";

    pid_t other = is_parent ? child : getppid();
    
    check(sigprocmask(SIG_SETMASK, &old_mask, nullptr));
    
    sigset_t wait_guess, wait_result;
    sigemptyset(&wait_guess);
    sigemptyset(&wait_result);

    const int ROUNDS = 10;

    for (int round = 1; round <= ROUNDS && !terminate_flag; ++round) {
        bool i_am_guesser = (round % 2 == (is_parent ? 0 : 1));

        if (!i_am_guesser) {
            int secret = rand() % N + 1;
            int attempts = 0;
            
            cout << endl;
            
            flockfile(stdout);
            cout << "[(" << role << ") PID " << getpid() << "] Загадал число " << secret << endl << flush;
            funlockfile(stdout);

            while (!terminate_flag) {
                wait_for(guess_ready, wait_guess);

                attempts++;
                
                flockfile(stdout);
                cout << "[(" << role << ") PID " << getpid() << "] Получил: " << guess_value << endl << flush;
                funlockfile(stdout);

                if (guess_value == secret) {
                    kill(other, SIG_WIN);
                    
                    flockfile(stdout);
                    cout << "[(" << role << ") PID " << getpid()
                         << "] Угадано за " << attempts << " попыток" << endl << flush;
                         funlockfile(stdout);
                    break;
                } else {
                    kill(other, SIG_LOSE);
                }
            }

        } else {
            int attempts = 0;

            flockfile(stdout);
            cout << "[(" << role << ") PID " << getpid() << "] Начинаю угадывать" << endl << flush;
            funlockfile(stdout);
  
            vector<bool> used(N + 1, false);

            while (!terminate_flag) {
                int guess;

                do {
                    guess = rand() % N + 1;
                } while (used[guess]);

                used[guess] = true;
                attempts++;

                flockfile(stdout);
                cout << "[(" << role << ") PID " << getpid() << "] Пытаюсь: " << guess << endl << flush;
                funlockfile(stdout);

                send_guess(other, guess);

                wait_for(result_ready, wait_result);

                if (result) {
                    flockfile(stdout);
                    cout << "[(" << role << ") PID " << getpid()
                         << "] Угадал за " << attempts << " попыток" << endl << flush;
                    funlockfile(stdout);
                    break;
                }
            }
        }
    }

    if (is_parent) {
        kill(child, SIGTERM);
        wait(nullptr);
    }

    cout << "[(" << role << ") PID " << getpid() << "] Завершение" << endl << flush;
    return 0;
}
