#include "api.h"

#define PORTNUM 7070
#define MAX_CHILDREN 64

void sendMsg(int sock, const char *msg)
{
    if (write(sock, msg, strlen(msg)) < 0) {
        fprintf(stderr, "[%d] Failed write(): [%d] %s\n", getpid(), errno, strerror(errno));
    }
    if (write(sock, "\n", 1) < 0) {
        fprintf(stderr, "[%d] Failed write(): [%d] %s\n", getpid(), errno, strerror(errno));
    }
}

int startListening(int mysocket)
{
    struct sockaddr_in serv;
    int res;

    memset(&serv, 0, sizeof(serv));
    serv.sin_family = AF_INET;
    serv.sin_addr.s_addr = htonl(INADDR_ANY);
    serv.sin_port = htons(PORTNUM);

    if (bind(mysocket, (struct sockaddr *)&serv, sizeof(struct sockaddr)) < 0) {
        fprintf(stderr, "[%d] Failed bind(): [%d] %s\n", getpid(), errno, strerror(errno));
        return EXIT_FAILURE;
    }

    if (listen(mysocket, MAX_CHILDREN) < 0) {
        fprintf(stderr, "[%d] Failed listen(): [%d] %s\n", getpid(), errno, strerror(errno));
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

unsigned short int trimLinebreaks(char *s)
{
    int i;
    int len = strlen(s);
    int found = 0;

    for (i = 0; i < len; i++) {
        if (s[i] == 10 || s[i] == 13) {
            s[i] = 0;
            found = 1;
            break;
        }
    }

    return found;
}

void * initThread(void *arg) {
    int consocket = *(int*)arg;
    char buf[MAX_INPUT_SIZE];
    char input[MAX_INPUT_SIZE];

    memset(input, 0, MAX_INPUT_SIZE);
    memset(buf, 0, MAX_INPUT_SIZE);

    pthread_mutex_lock(&mutex_stats);
    stats.running_children++;
    stats.total_connections++;
    pthread_mutex_unlock(&mutex_stats);

    while (read(consocket, buf, MAX_INPUT_SIZE) > 0) {
        int found_break = trimLinebreaks(buf);
        strcat(input, buf); //TODO: This may override input length limit

        if (!found_break) {
            if (strlen(input) > MAX_INPUT_SIZE) {
                printf("[%d] Bad incoming message (no linebreak in first %d chars), exiting\n; Message: %s\n", getpid(), MAX_INPUT_SIZE, buf);
                break;
            }

            memset(buf, 0, MAX_INPUT_SIZE);
            continue;
        }

        stats.total_requests++;
        int next_action = handleComand(consocket, input);
        if (next_action == COMMAND_EXIT) {
            break;
        }

        memset(input, 0, MAX_INPUT_SIZE);
        memset(buf, 0, MAX_INPUT_SIZE);
    }

    close(consocket);

    pthread_mutex_lock(&mutex_stats);
    stats.running_children--;
    pthread_mutex_unlock(&mutex_stats);

    return NULL;
}

int initDemon()
{
    struct sockaddr_in dest;
    int mysocket;
    socklen_t socksize = sizeof(struct sockaddr_in);
    pthread_t tid;
    int running_children;

    if ((mysocket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        fprintf(stderr, "[%d] Failed socket(): [%d] %s\n", getpid(), errno, strerror(errno));
        return EXIT_FAILURE;
    }

    int optval = 1;
    if (setsockopt(mysocket, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
        fprintf(stderr, "[%d] Failed setsockopt(SO_REUSEADDR): [%d] %s\n", getpid(), errno, strerror(errno));
        return EXIT_FAILURE;
    }

    if (startListening(mysocket) != 0) {
        return EXIT_FAILURE;
    }

    printf("# Listening on %d; PID: %d\n", PORTNUM, getpid());

    int consocket = -1;
    char err[100];

    while (consocket) {
        consocket = accept(mysocket, (struct sockaddr *)&dest, &socksize);
        if (consocket < 0) {
            fprintf(stderr, "[%d] Failed accept(); %d : %s\n", getpid(), errno, strerror(errno));
            continue;
        }

        optval = 1;
        if (setsockopt(consocket, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof(optval)) < 0) {
            fprintf(stderr, "[%d] Failed setsockopt(TCP_NODELAY): [%d] %s\n", getpid(), errno, strerror(errno));
            return EXIT_FAILURE;
        }

        pthread_mutex_lock(&mutex_stats);
        running_children = stats.running_children;
        pthread_mutex_unlock(&mutex_stats);

        if (running_children >= MAX_CHILDREN) {
            sprintf(err, "Too many connections (%d)", MAX_CHILDREN);
            responseError(consocket, err);
            printf("[%d] Have %d of %d children, dropping incoming connection\n", getpid(), running_children, MAX_CHILDREN);
            close(consocket);
            continue;
        }

        /* TODO: Paranoia says consocket value could be rewritten by parent thread before it will be read in child */
        if (pthread_create(&tid, NULL, &initThread, &consocket)) {
            responseError(consocket, "Failed to create thread");
            printf("[%d] Failed to create thread for connection from %s\n", getpid(), inet_ntoa(dest.sin_addr));
            close(consocket);
        }
        pthread_join(tid, NULL);
    }

    close(mysocket);
    return EXIT_SUCCESS;
}
