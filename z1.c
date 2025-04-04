#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>

#define ROUNDS 10

volatile sig_atomic_t guessed = 0;         // Флаг: число угадано?
volatile sig_atomic_t opponent_pid = 0;    // PID второго игрока
volatile sig_atomic_t number_to_guess = 0; // Загаданное число
volatile sig_atomic_t attempts = 0;        // Количество попыток

void handle_guess(int sig, siginfo_t *info, void *context) {
    (void)sig;
    (void)context;
    int guess = info->si_value.sival_int;

    printf("[Игрок 1] Получил число %d от Игрока 2\n", guess);
    
    if (guess == number_to_guess) {
        guessed = 1;
        kill(opponent_pid, SIGUSR1); // Сообщаем, что число угадано
    } else {
        kill(opponent_pid, SIGUSR2);
    }
}

void handle_result(int sig) {
    if (sig == SIGUSR1) {
        guessed = 1;
    }
}

void wait_for_signal() {
    sigset_t mask;
    sigemptyset(&mask);
    sigsuspend(&mask);
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
        attempts = 0;
        if (role == 0) {
            number_to_guess = rand() % max_number + 1;
            printf("[Игрок 1] Загадал число %d\n", number_to_guess);
            kill(opponent_pid, SIGUSR2);

            while (!guessed) wait_for_signal();
            printf("[Игрок 1] Число угадано за %d попыток!\n", attempts);
            guessed = 0;
            role = 1;
        } else {
            guessed = 0;
            while (!guessed) {
                int guess = rand() % max_number + 1;
                printf("[Игрок 2] Пробую %d\n", guess);
                attempts++;

                union sigval value;
                value.sival_int = guess;
                sigqueue(opponent_pid, SIGRTMIN, value);

                wait_for_signal();
            }
            printf("[Игрок 2] Угадал число за %d попыток!\n", attempts);
            guessed = 0;
            role = 0;
        }
    }

    printf("[Процесс %d] Завершаю игру...\n", getpid());
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
        waitpid(pid, &status, 0);
        printf("Оба процесса завершены. Выход в консоль.\n");
    }

    return 0;
}
