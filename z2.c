#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/wait.h>

#define MAX_ROUNDS 10
#define MSGSZ sizeof(struct message) - sizeof(long)

typedef struct message {
    long mtype;
    int value;
} message_t;

enum {
    MSG_NUMBER = 1,
    MSG_GUESS  = 2,
    MSG_RESULT = 3
};

void send_msg(int qid, long type, int value) {
    message_t msg = { .mtype = type, .value = value };
    if (msgsnd(qid, &msg, MSGSZ, 0) == -1) {
        perror("msgsnd");
        exit(EXIT_FAILURE);
    }
}

int recv_msg(int qid, long type) {
    message_t msg;
    if (msgrcv(qid, &msg, MSGSZ, type, 0) == -1) {
        perror("msgrcv");
        exit(EXIT_FAILURE);
    }
    return msg.value;
}

void player_1(int qid, int max_number) {
    srand(time(NULL) ^ getpid());

    for (int round = 1; round <= MAX_ROUNDS; ++round) {
        int number = rand() % max_number + 1;
        int guess, attempts = 0;

        printf("[Раунд %d]\n", round);
        printf("[Игрок 1] Загадал число %d\n", number);

        send_msg(qid, MSG_NUMBER, number);

        while (1) {
            guess = recv_msg(qid, MSG_GUESS);
            attempts++;
            printf("[Игрок 1] Получено число %d от Игрока 2\n", guess);

            int result = (guess == number) ? 1 : 0;
            send_msg(qid, MSG_RESULT, result);

            if (result == 1) {
                printf("[Игрок 1] Игрок 2 угадал за %d попыток!\n\n", attempts);
                break;
            }
        }
    }

    send_msg(qid, MSG_NUMBER, -1);
}

void player_2(int qid, int max_number) {
    while (1) {
        int number = recv_msg(qid, MSG_NUMBER);
        if (number == -1) {
            printf("[Игрок 2] Получен сигнал завершения игры.\n");
            break;
        }

        int guess, low = 1, high = max_number, result;

        printf("[Игрок 2] Угадываю число...\n");

        while (1) {
            guess = (low + high) / 2;
            printf("[Игрок 2] Пробую %d\n", guess);

            send_msg(qid, MSG_GUESS, guess);
            result = recv_msg(qid, MSG_RESULT);

            if (result == 1) {
                printf("[Игрок 2] Угадал число %d!\n\n", guess);
                break;
            }

            if (guess < number)
                low = guess + 1;
            else
                high = guess - 1;
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Использование: %s <макс_число>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int max_number = atoi(argv[1]);
    if (max_number <= 1) {
        fprintf(stderr, "Максимальное число должно быть больше 1.\n");
        exit(EXIT_FAILURE);
    }

    key_t key = ftok(".", 'G');
    if (key == -1) {
        perror("ftok");
        exit(EXIT_FAILURE);
    }

    int qid = msgget(key, IPC_CREAT | 0666);
    if (qid == -1) {
        perror("msgget");
        exit(EXIT_FAILURE);
    }

    pid_t pid = fork();

    if (pid < 0) {
        perror("fork");
        msgctl(qid, IPC_RMID, NULL);
        exit(EXIT_FAILURE);
    }

    if (pid == 0) {
        player_2(qid, max_number);
        exit(EXIT_SUCCESS);
    } else {
        player_1(qid, max_number);
        wait(NULL);
        msgctl(qid, IPC_RMID, NULL);
        printf("Игра завершена. Очередь сообщений удалена.\n");
    }

    return 0;
}
