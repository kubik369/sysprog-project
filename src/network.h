#ifndef NETWORK_H_ 
#define NETWORK_H_

/* Use real lengths, \0 is accounted for in the code. */
#define USERNAME_LEN 10
#define HOST_LEN 15
#define MAX_MESSAGE_LEN 70

#define BUFFER_SIZE 1024

/* Sleep time after each chat cycle in nanoseconds */
#define SLEEP_TIME 10000000

/* select() blocking timeout in seconds */
#define TIMEOUT 30

typedef struct settings {
    char *username, *remote_username, *host;
    int port;
} settings;

struct sockaddr_in* init_server(int sock, struct settings *prefs);

struct sockaddr_in* init_client(int sock, struct settings *prefs);

#endif
