#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>
#include <string.h>

/* use real lengths */
#define USERNAME_LEN 10
#define HOST_LEN 15

int parse_args(int argc, char *argv[], int *port, char **username, char **host);

int main(int argc, char *argv[])
{
    int port = -1;
    char *host = NULL, *username = NULL;

    if (parse_args(argc, argv, &port, &username, &host) == -1) {
        printf("Some error\n");
        return 1;
    }

    printf("Port number: %d\nUsername: %s\nHost: %s\n", port, username, host);

    if (port == -1 || username == NULL) {
        printf("Port and username are required arguments!\n");
        return 1;
    }
    if (host != NULL) {
        free(host);
    }
    if (username != NULL) {
        free(username);
    }

    return 0;
}

int parse_args(int argc, char *argv[], int *port, char **username, char **host) {
    int option;

    while ((option = getopt(argc, argv, "p:h:u:")) != -1) {
        switch (option) {
            case 'p': {
                *port = atoi(optarg);
                if (*port <= 0 || *port > 65535) {
                    printf("Invalid port number!\n");
                    return -1;
                }
                break;
            }
            case 'u': {
                if (strlen(optarg) > USERNAME_LEN) {
                    printf("Chosen username is too long!\n");
                    return -1;
                }
                *username = (char*)malloc((USERNAME_LEN + 1) * sizeof(char));
                strcpy(*username, optarg);
                break;
            }
            case 'h': {
                if (strlen(optarg) > HOST_LEN) {
                    printf("Chosen host adress is too long!");
                    return -1;
                }
                *host = (char*)malloc((HOST_LEN + 1) * sizeof(char));
                strcpy(*host, optarg);
                break;
            }
            default: {
                printf("Usage: utalk -h host -p port_number -u username");
                return -1;
            }
        }
    }
    return 0;
}