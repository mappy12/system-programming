#include "check.hpp"
#include <iostream>
#include <cstdlib>
#include <ctime>
#include <cerrno>
#include <unistd.h>
#include <sys/wait.h>
#include <mqueue.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <csignal>

using namespace std;

const char* MQ_PARENT = "/lab2_mq_parent";
const char* MQ_CHILD  = "/lab2_mq_child";

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

bool mq_send_int(mqd_t mq, int value, pid_t peer) {
    while (true) {
        if (!is_alive(peer)) return false;

        int res = mq_send(mq, (const char*)&value, sizeof(value), 0);
        
        if (res == 0) return true;            
        if (errno == EAGAIN) continue;
        
        check(res);                           
    }
}

bool mq_receive_int(mqd_t mq, int& value, pid_t peer) {
    while (true) {
        if (!is_alive(peer)) return false;

        ssize_t n = mq_receive(mq, (char*)&value, sizeof(value), nullptr);
        
        if (n == sizeof(value)) return true; 
        if (errno == EAGAIN) continue;
        
        check(n);
    }
}

void riddler(int secret, mqd_t read_mq, mqd_t write_mq, pid_t peer, int round) {
    cout << "Раунд " << round << ". PID " << getpid() << " загадал число.\n";
    fflush(stdout);

    while (is_alive(peer)) {
        int guess;
        
        if (!mq_receive_int(read_mq, guess, peer)) break;

        cout << "Раунд " << round << ". PID " << getpid() << " получил: " << guess << "\n";
        fflush(stdout);

        int correct = (guess == secret);
        
        if (!mq_send_int(write_mq, correct, peer)) break;
        if (correct) break;
    }
}

void guesser(int N, mqd_t write_mq, mqd_t read_mq, pid_t peer, int round) {
    int attempts = 0;

    while (is_alive(peer)) {
        int guess = rand() % N + 1;
        
        attempts++;

        cout << "Раунд " << round << ". PID " << getpid() << " отправил: " << guess << "\n";
        fflush(stdout);

        if (!mq_send_int(write_mq, guess, peer)) break;

        int response;
        
        if (!mq_receive_int(read_mq, response, peer)) break;

        if (response) {
            cout << "Раунд " << round << ". PID " << getpid()
                 << " угадал число " << guess
                 << " за " << attempts << " попыток.\n";
            fflush(stdout);
            break;
        }
    }
}

void play_game(bool is_parent, mqd_t read_mq, mqd_t write_mq, int N, int rounds, pid_t peer) {
    for (int r = 1; r <= rounds && is_alive(peer); ++r) {
        bool i_am_riddler = (r % 2 == (is_parent ? 1 : 0));
        int secret = rand() % N + 1;

        if (i_am_riddler)
            riddler(secret, read_mq, write_mq, peer, r);
        else
            guesser(N, write_mq, read_mq, peer, r);
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

    struct mq_attr attr{};
    attr.mq_maxmsg = 10;
    attr.mq_msgsize = sizeof(int);

    mq_unlink(MQ_PARENT);
    mq_unlink(MQ_CHILD);

    mqd_t mq_parent = check(mq_open(MQ_PARENT, O_CREAT | O_RDWR | O_NONBLOCK, 0666, &attr));
    mqd_t mq_child  = check(mq_open(MQ_CHILD,  O_CREAT | O_RDWR | O_NONBLOCK, 0666, &attr));

    struct sigaction sa{};
    sa.sa_handler = sigchld_handler;
    sigaction(SIGCHLD, &sa, nullptr);

    pid_t pid = check(fork());
    if (pid == 0) {
        pid_t parent_id = getppid();
        
        mqd_t read_mq  = check(mq_open(MQ_PARENT, O_RDONLY | O_NONBLOCK));
        mqd_t write_mq = check(mq_open(MQ_CHILD,  O_WRONLY | O_NONBLOCK));

        play_game(false, read_mq, write_mq, N, rounds, parent_id);

        check(mq_close(read_mq));
        check(mq_close(write_mq));

        cout << "Ребенок завершен\n";
        return 0;
    }

    // PARENT
    mqd_t read_mq  = check(mq_open(MQ_CHILD,  O_RDONLY | O_NONBLOCK));
    mqd_t write_mq = check(mq_open(MQ_PARENT, O_WRONLY | O_NONBLOCK));

    play_game(true, read_mq, write_mq, N, rounds, pid);

    check(mq_close(read_mq));
    check(mq_close(write_mq));

    waitpid(pid, nullptr, 0);

    mq_unlink(MQ_PARENT);
    mq_unlink(MQ_CHILD);

    cout << "Родитель завершен\n";
    return 0;
}
