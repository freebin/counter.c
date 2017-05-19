/* ===== Global vars and typedefs ===== */

#define DEBUG_ENABLED 0

#define COMMAND_CONTINUE 0
#define COMMAND_EXIT 1

#define MAX_INPUT_SIZE 255

char *MSG_ACTION_EXIT = "exit";
char *MSG_ACTION_STATS = "stats";
char *MSG_ACTION_INCREMENT = "increment";
char *MSG_ACTION_GET = "get";

typedef struct {
    char *action; //Action string, one of MSG_ACTION_*
    unsigned int num;
} S_MESSAGE;

typedef struct {
    unsigned int total_connections;
    unsigned int total_requests;
    unsigned int started_at;
    unsigned int running_children;
    unsigned int num;
    double timer;
} S_STATS;

S_STATS stats;

pthread_mutex_t mutex_stats, mutex_data;

/* ===== Methods ===== */

/**
 * IN: Socket to write, raw requested string
 * OUT: (bool) close_connection flag
 */
int handleComand(int, char*);

/**
 * IN: Socket to write, error message string
 */
void responseError(int socket, char *err_msg);

/**
 * IN: Socket to write
 */
void responseSuccess(int socket);

/**
 * IN: Raw message string, pointer to S_MESAGE structure to write to
 * OUT: EXIT_SUCCESS or EXIT_FAILURE
 */
unsigned short int decodeMessage(const char*, S_MESSAGE*);

/**
 * IN: Socket to write
 */
void showStats(int socket);

/**
 * IN: Socket to write, pointer to S_MESSAGE containing increment num
 */
void incrementCounter(int socket, S_MESSAGE *request);

/**
 * IN: Socket to write
 */
void getCounter(int socket);

/**
 * IN: Returns seconds with microseconds since Epoch
 */
double getMicroTime(void);
