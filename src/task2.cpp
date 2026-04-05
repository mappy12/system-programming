#include <iostream>
#include <unistd.h>
#include <sys/wait.h>
#include <cstdlib>
#include <ctime>
#include <csignal>

using namespace std;

volatile sig_atomic_t child_exited = 0;

void handle_sigchld(int) { child_exited = 1; }

int random_guess(int N) { return rand() % N + 1; }

int main(int argc, char* argv[]) {
    if(argc < 3) { 
        cerr << "Usage: " << argv[0] << " N rounds\n"; 
        return 1; 
    }

    int N = atoi(argv[1]);
    int ROUNDS = atoi(argv[2]);

    if(N <= 0 || ROUNDS <= 0) { 
        cerr << "N and rounds must be > 0\n"; 
        return 1; 
    }

    signal(SIGCHLD, handle_sigchld);

    bool parent_guesses = false; // чередование ролей

    for(int round = 1; round <= ROUNDS; round++) {
        cout << "\n=== Round " << round << " ===\n";

        int fd[2];
        if(pipe(fd) == -1) { perror("pipe"); return 1; }

        pid_t pid = fork();
        if(pid < 0) { perror("fork"); return 1; }

        if(pid == 0) {
            // CHILD
            srand(time(nullptr) ^ getpid());
            close(fd[1]);

            int secret;
            if(read(fd[0], &secret, sizeof(secret)) <= 0) { 
                cerr << "Child failed to read\n"; 
                exit(1); 
            }

            int attempts = 0;
            if(parent_guesses) {
                // Ребёнок загадывает, Parent угадывает
                cout << "Child: I chose the number " << secret << endl;
                int guess;
                do {
                    guess = random_guess(N);
                    attempts++;
                    cout << "Parent guesses: " << guess << endl;
                } while(guess != secret);
                cout << "Parent guessed the number in " << attempts << " attempts.\n";
            } else {
                // Ребёнок угадывает
                cout << "Child: I need to guess a number between 1 and " << N << endl;
                int guess;
                do {
                    guess = random_guess(N);
                    attempts++;
                    cout << "Child guesses: " << guess << endl;
                } while(guess != secret);
                cout << "Child guessed the number " << secret 
                     << " in " << attempts << " attempts.\n";
            }

            close(fd[0]);
            exit(0);
        } else {
            // PARENT
            srand(time(nullptr) ^ getpid());
            close(fd[0]);

            int secret = rand() % N + 1;
            if(write(fd[1], &secret, sizeof(secret)) <= 0) 
                cerr << "Parent failed to write\n";
            close(fd[1]);

            while(!child_exited) {
                pid_t w = waitpid(pid, nullptr, WNOHANG);
                if(w == pid) break;
            }

            child_exited = 0;
            parent_guesses = !parent_guesses;
            cout << "Parent: Round finished.\n";
        }
    }

    cout << "\n=== Game Over ===\n";
    return 0;
}
