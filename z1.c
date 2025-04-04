#define _GNU_SOURCE  
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <sys/wait.h>

#define ROUNDS 10  

volatile sig_atomic_t guessed_number = 0;
volatile sig_atomic_t guessed = 0;
volatile pid_t opponent_pid = 0;

void handle_guess(int sig, siginfo_t *info, void *context) {
    (void)sig;
    (void)context;
    guessed_number = info->si_value.sival_int;
    opponent_pid = info->si_pid;
}

void handle_result(int sig) {
    guessed = (sig == SIGUSR1);  
}

void play_game(int max_number, int role) {
    struct sigaction sa_guess, sa_result;
    
    sa_guess.sa_flags = SA_SIGINFO;
    sa_guess.sa_sigaction = handle_guess;
    sigaction(SIGRTMIN, &sa_guess, NULL);
    
    sa_result.sa_flags = 0;
    sa_result.sa_handler = handle_result;
    sigaction(SIGUSR1, &sa_result, NULL);
    sigaction(SIGUSR2, &sa_result, NULL);

    srand(time(NULL) ^ getpid());

    for (int round = 0; round < ROUNDS; round++) {
        int target, attempts = 0;

        if (role == 0) {
            target = rand() % max_number + 1;
            printf("[Игрок 1] Загадал число %d\n", target);
            kill(opponent_pid, SIGUSR2);

            while (!guessed) pause();
            printf("[Игрок 1] Число угадано за %d попыток!\n", attempts);
            guessed = 0;
            role = 1;
            kill(opponent_pid, SIGUSR2);
        } else {
            guessed = 0;
            attempts = 0;
            while (!guessed) {
                int guess = rand() % max_number + 1;
                printf("[Игрок 2] Пробую %d\n", guess);
                attempts++;

                union sigval value;
                value.sival_int = guess;
                sigqueue(opponent_pid, SIGRTMIN, value);

                pause();
            }
            printf("[Игрок 2] Угадал за %d попыток!\n", attempts);
            guessed = 0;
            role = 0;
            kill(opponent_pid, SIGUSR2);
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Использование: %s <N>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int max_number = atoi(argv[1]);
    if (max_number <= 0) {
        printf("Число N должно быть больше 0!\n");
        exit(EXIT_FAILURE);
    }

    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        exit(EXIT_FAILURE);
    } else if (pid == 0) {
        opponent_pid = getppid();
        play_game(max_number, 1);
        exit(0);
    } else {
        opponent_pid = pid;
        play_game(max_number, 0);
        
        int status;
        waitpid(pid, &status, 0);  // Ждем завершения дочернего процесса
    }

    return 0;
}
