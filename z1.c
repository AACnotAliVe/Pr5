#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <string.h>
#include <sys/wait.h>

#define GAME_ROUNDS 10

volatile sig_atomic_t signal_received = 0;
volatile sig_atomic_t last_received_number = 0;
volatile sig_atomic_t guess_was_correct = 0;

void on_guess_received(int sig, siginfo_t *info, void *ctx) {
    if (sig == SIGRTMIN) {
        last_received_number = info->si_value.sival_int;
        signal_received = 1;
    }
}

void on_result_signal(int sig) {
    guess_was_correct = (sig == SIGUSR1) ? 1 : 0;
}

void start_sender(pid_t guesser_pid, int max_value, int round_number) {
    srand(time(NULL) ^ getpid());
    int secret_number = 1 + rand() % max_value;
    printf("🔒 [Раунд %d] Игрок 1 загадал число: %d\n", round_number, secret_number);
    fflush(stdout);
    struct sigaction sa = {0};
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = on_guess_received;
    sigaction(SIGRTMIN, &sa, NULL);

    sigset_t wait_mask;
    sigemptyset(&wait_mask);
    sigaddset(&wait_mask, SIGRTMIN);
    sigprocmask(SIG_BLOCK, &wait_mask, NULL);

    int attempts = 0;

    while (1) {
        signal_received = 0;

        siginfo_t info;
        sigwaitinfo(&wait_mask, &info);

        int guess = info.si_value.sival_int;
        attempts++;
        printf("Игрок 1 получил число: %d\n", guess);
        fflush(stdout);

        if (guess == secret_number) {
            kill(guesser_pid, SIGUSR1);
            break;
        } else {
            kill(guesser_pid, SIGUSR2);
        }
    }

    printf("Игрок 1: число отгадано за %d попыток.\n\n", attempts);
}

void start_guesser(pid_t sender_pid, int max_value) {
    struct sigaction sa = {0};
    sa.sa_handler = on_result_signal;
    sigaction(SIGUSR1, &sa, NULL);
    sigaction(SIGUSR2, &sa, NULL);

    sigset_t wait_mask;
    sigemptyset(&wait_mask);
    sigaddset(&wait_mask, SIGUSR1);
    sigaddset(&wait_mask, SIGUSR2);
    sigprocmask(SIG_BLOCK, &wait_mask, NULL);

    for (int round = 1; round <= GAME_ROUNDS; ++round) {
        int attempts = 0;

        do {
            int guess = 1 + rand() % max_value;
            printf("Игрок 2 пробует: %d\n", guess);
            fflush(stdout);

            union sigval value;
            value.sival_int = guess;
            sigqueue(sender_pid, SIGRTMIN, value);

            int sig;
            sigwait(&wait_mask, &sig);
            attempts++;
        } while (!guess_was_correct);

        printf("Игрок 2: число угадано за %d попыток!\n\n", attempts);
        guess_was_correct = 0;
        sleep(1);
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Использование: %s <максимальное число>\n", argv[0]);
        return EXIT_FAILURE;
    }

    int max_value = atoi(argv[1]);
    if (max_value < 1) {
        fprintf(stderr, "Число должно быть больше 0.\n");
        return EXIT_FAILURE;
    }

    pid_t child_pid = fork();

    if (child_pid < 0) {
        perror("Ошибка fork");
        return EXIT_FAILURE;
    }

    if (child_pid == 0) {
        srand(time(NULL) ^ getpid());
        start_guesser(getppid(), max_value);
        exit(EXIT_SUCCESS);
    } else {
        for (int round = 1; round <= GAME_ROUNDS; ++round) {
            start_sender(child_pid, max_value, round);
        }

        kill(child_pid, SIGTERM);
        wait(NULL);
        printf("Игра завершена. Процессы завершились корректно.\n");
    }

    return EXIT_SUCCESS;
}
