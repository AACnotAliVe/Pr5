#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <mqueue.h>
#include <time.h>
#include <sys/wait.h>

#define QUEUE_NAME "/guess_game_queue"
#define ROUNDS 10

typedef struct {
    int guess;
    int result;
} Message;

void play_game(int max_number, int role) {
    mqd_t mq;
    struct mq_attr attr;
    Message msg;
    
    attr.mq_flags = 0;
    attr.mq_maxmsg = 10;
    attr.mq_msgsize = sizeof(Message);
    attr.mq_curmsgs = 0;

    mq = mq_open(QUEUE_NAME, O_CREAT | O_RDWR, 0644, &attr);
    if (mq == (mqd_t)-1) {
        perror("Ошибка открытия очереди");
        exit(1);
    }

    srand(time(NULL) ^ getpid());
    for (int round = 0; round < ROUNDS; round++) {
        int target, attempts = 0;

        if (role == 0) {
            target = rand() % max_number + 1;
            printf("[Игрок 1] Загадал число %d\n", target);

            while (1) {
                mq_receive(mq, (char*)&msg, sizeof(Message), NULL);
                attempts++;

                if (msg.guess == target) {
                    msg.result = 1;
                    mq_send(mq, (char*)&msg, sizeof(Message), 0);
                    printf("[Игрок 1] Число %d угадано за %d попыток!\n", target, attempts);
                    break;
                } else {
                    msg.result = 0;
                    mq_send(mq, (char*)&msg, sizeof(Message), 0);
                }
            }

            role = 1;
        } else {
            attempts = 0;
            while (1) {
                msg.guess = rand() % max_number + 1;
                printf("[Игрок 2] Пробую %d\n", msg.guess);
                mq_send(mq, (char*)&msg, sizeof(Message), 0);
                attempts++;

                mq_receive(mq, (char*)&msg, sizeof(Message), NULL);

                if (msg.result == 1) {
                    printf("[Игрок 2] Угадал число за %d попыток!\n", attempts);
                    break;
                }
            }

            role = 0;
        }
    }

    mq_close(mq);
    if (role == 0) {
        mq_unlink(QUEUE_NAME);
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
        play_game(max_number, 1);
        exit(0);
    } else {
        play_game(max_number, 0);
        
        int status;
        waitpid(pid, &status, 0);
    }

    return 0;
}
