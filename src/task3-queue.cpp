#include <iostream>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/wait.h>
#include <unistd.h>
#include <cstdlib>
#include <ctime>

using namespace std;

struct Message {
    long mtype;
    int value;
};

int random_guess(int N) {
    return rand() % N + 1;
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        cerr << "Использование: " << argv[0] << " N rounds\n";
        return 1;
    }

    int N = atoi(argv[1]);
    int ROUNDS = atoi(argv[2]);

    if (N <= 0 || ROUNDS <= 0) {
        cerr << "N и rounds должны быть > 0\n";
        return 1;
    }

    srand(time(nullptr) ^ getpid());

    int msgid = msgget(IPC_PRIVATE, 0666 | IPC_CREAT);
    if (msgid < 0) {
        perror("msgget");
        return 1;
    }

    bool parent_guesses = false;

    for (int round = 1; round <= ROUNDS; round++) {
        cout << "\n=== Раунд " << round << " ===\n";

        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            return 1;
        }

        if (pid == 0) {
            srand(time(nullptr) ^ getpid());

            Message msg;
            if (msgrcv(msgid, &msg, sizeof(int), 1, 0) < 0) {
                perror("msgrcv");
                exit(1);
            }

            int secret = msg.value;
            int attempts = 0;

            if (parent_guesses) {
                cout << "Ребёнок: Я загадал число " << secret << endl;
                int guess;
                do {
                    guess = random_guess(N);
                    attempts++;
                    cout << "Родитель пытается: " << guess << endl;
                } while (guess != secret);
                cout << "Родитель угадал число за " << attempts << " попыток.\n";
            } else {
                cout << "Ребёнок: Мне нужно угадать число от 1 до " << N << endl;
                int guess;
                do {
                    guess = random_guess(N);
                    attempts++;
                    cout << "Ребёнок пробует: " << guess << endl;
                } while (guess != secret);
                cout << "Ребёнок угадал число " << secret << " за " << attempts << " попыток.\n";
            }

            exit(0);
        } else {
            int secret = rand() % N + 1;

            Message msg;
            msg.mtype = 1;
            msg.value = secret;

            if (msgsnd(msgid, &msg, sizeof(int), 0) < 0) {
                perror("msgsnd");
                return 1;
            }

            waitpid(pid, nullptr, 0);

            parent_guesses = !parent_guesses;
            cout << "Родитель: Раунд завершён.\n";
        }
    }

    msgctl(msgid, IPC_RMID, nullptr);

    cout << "\n=== Игра окончена ===\n";
    return 0;
}
