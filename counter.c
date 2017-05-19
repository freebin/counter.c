#include "headers.h"
#include "demon.c"

#define WRITE_COUNTER_MIN_DIFF 10000

char *COUNTER_FILE_PATH = "/local/tmp/the_counter";//TODO: Take from param on start
unsigned int last_saved_num = 0;

int initStats(void);
void writeCounter(void);
void freeMessage(S_MESSAGE *msg);

int main(int argc, char *argv[])
{
    if (initStats() != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }

    return initDemon();
}

int initStats() {
    if (pthread_mutex_init(&mutex_stats, NULL) != 0) {
        printf("Failed to init STATS mutex\n");
        return EXIT_FAILURE;
    }

    stats.total_connections = 0;
    stats.total_requests = 0;
    stats.running_children = 0;
    stats.started_at = (int)time(NULL);

    FILE *fd = NULL;
    fd = fopen(COUNTER_FILE_PATH, "r");
    if (fd == NULL) {
        stats.num = 0;
    } else {
        char buf[40] = "";
        if (fread(buf, 40, 1, fd)) {
            if(feof(fd)) printf("Premature end of file.");
            else printf("File read error.");
        }
        fclose(fd);
        stats.num = atoi(buf);
    }
    last_saved_num = stats.num;

    return EXIT_SUCCESS;
}

void writeCounter(void) {
    char buf[40] = "";

    FILE *fd = NULL;
    fd = fopen(COUNTER_FILE_PATH, "w");
    if (fd == NULL) {
        printf("Failed to open for write %s\n", COUNTER_FILE_PATH);
    }

    sprintf(buf, "%d", stats.num);
    if (!fwrite(buf, strlen(buf), 1, fd)) {
        printf("Failed to write %d to %s\n", stats.num, COUNTER_FILE_PATH);
    }

    fclose(fd);
}

int handleComand(int socket, char *msg)
{
    S_MESSAGE request;
    request.num = 0;
    request.action = "";

    unsigned int err;
    if (decodeMessage(msg, &request) == EXIT_FAILURE) {
        responseError(socket, "Something went wrong");
        freeMessage(&request);
        return COMMAND_EXIT;
    }

    if (strlen(request.action) == 0) {
        freeMessage(&request);
        return COMMAND_EXIT;
    }

    if (!strcmp(request.action, MSG_ACTION_EXIT)) {
        responseSuccess(socket);
        freeMessage(&request);
        return COMMAND_EXIT;
    } else if (!strcmp(request.action, MSG_ACTION_STATS)) {
        showStats(socket);
    } else if (!strcmp(request.action, MSG_ACTION_INCREMENT)) {
        incrementCounter(socket, &request);
        freeMessage(&request);
        return COMMAND_EXIT;
    } else if (!strcmp(request.action, MSG_ACTION_GET)) {
        getCounter(socket);
        freeMessage(&request);
        return COMMAND_EXIT;
    } else {
        char tmp[255];
        sprintf(tmp, "Unknown action '%s'", request.action);
        responseError(socket, tmp);
    }

    freeMessage(&request);
    return COMMAND_EXIT;
}

void freeMessage(S_MESSAGE *msg) {
    free(msg->action);
}

unsigned short int decodeMessage(const char *str, S_MESSAGE *message) {
    unsigned int i, j;
    unsigned short int in_field = 0;

    message->num = 0;
    message->action = malloc(255);
    if (message->action == NULL) {
        printf("Failed to allocate MESSAGE.ACTION\n");
        return EXIT_FAILURE;
    }

    unsigned int is_action = 1;
    char num_str[255];
    j = 0;
    for (i = 0; i < strlen(str); i++) {
        if (i == 254) {
            break;
        }
        if (str[i] == ':') {
            is_action = 0;
            message->action[i] = 0;
            continue;
        }

        if (is_action) {
            message->action[i] = str[i];
        } else {
            num_str[j] = str[i];
            j++;
        }
    }
    message->action[i] = 0;
    num_str[j] = 0;
    message->num = atoi(num_str);

    return EXIT_SUCCESS;
}

void responseError(int socket, char *err_msg) {
    sendMsg(socket, err_msg);
}

void responseSuccess(int socket) {
    sendMsg(socket, "ok");
}

void showStats(int socket) {
    char resp[1024] = "";
    char tmp[40] = "";

    strcat(resp, "tot_conns: ");
    sprintf(tmp, "%d\n", stats.total_connections);
    strcat(resp, tmp);

    strcat(resp, "tot_reqs: ");
    sprintf(tmp, "%d\n", stats.total_requests);
    strcat(resp, tmp);

    strcat(resp, "start: ");
    sprintf(tmp, "%d\n", stats.started_at);
    strcat(resp, tmp);

    strcat(resp, "chld_running: ");
    sprintf(tmp, "%d\n", stats.running_children);
    strcat(resp, tmp);

    strcat(resp, "the_counter: ");
    sprintf(tmp, "%d\n", stats.num);
    strcat(resp, tmp);

    sendMsg(socket, resp);
}

void incrementCounter(int socket, S_MESSAGE *request) {
    pthread_mutex_lock(&mutex_stats);
    stats.num = stats.num + request->num;
    pthread_mutex_unlock(&mutex_stats);

    responseSuccess(socket);

    //TODO: Do not report success if failed to write
    if ((stats.num - last_saved_num) >= WRITE_COUNTER_MIN_DIFF) {
        last_saved_num = stats.num;
        writeCounter();
    }
}

void getCounter(int socket) {
    char str[40];
    sprintf(str, "%d", stats.num);
    sendMsg(socket, str);
}

double getMicroTime(void) {
    struct timeval cur_time;

    if (gettimeofday(&cur_time, NULL) < 0) {
        fprintf(stderr, "[%d] Failed gettimeofday(): [%d] %s\n", getpid(), errno, strerror(errno));
        return 0;
    }

    return ((double)cur_time.tv_sec + ((double)cur_time.tv_usec / 1000000));
}
