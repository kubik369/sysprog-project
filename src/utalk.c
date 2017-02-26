#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>

/* File descriptor */
#define STDIN 0

/* Use real lengths, \0 is accounted for in the code. */
#define USERNAME_LEN 10
#define HOST_LEN 15

#define BUFFER_SIZE 1024

/* select() blocking timeout in seconds */
#define TIMEOUT 10

struct settings {
    char *username, *remote_username, *host;
    int port;
};

struct sockaddr_in* init_server(int sock, struct settings *prefs);

struct sockaddr_in* init_client(int sock, struct settings *prefs);

int parse_args(int argc, char *argv[], struct settings *options);

int main(int argc, char *argv[]) {
    struct settings prefs = { NULL, NULL, NULL, -1};

    if (parse_args(argc, argv, &prefs) == -1) {
        return 1;
    }

    printf("Port number: %d\nUsername: %s\nHost: %s\n", prefs.port, prefs.username, prefs.host);

    if (prefs.port == -1 || prefs.username == NULL) {
        perror("Port and username are required arguments!\n");
        return 1;
    }

    int sock = socket(AF_INET, SOCK_DGRAM, 0);

    if (sock == -1) {
        perror("Could not create a socket.\n");
        return 1;
    }
        
    struct sockaddr_in *remote_addr = (prefs.host == NULL)
        ? init_server(sock, &prefs)
        : init_client(sock, &prefs);

    fd_set write_fds, read_fds;
    int max_fd = sock + 1,
        select_fd = 0,
        bytes_received,
        status;
    socklen_t addr_len = sizeof(struct sockaddr);

    //struct timeval timeout = {TIMEOUT, 0};
    char input_buffer[BUFFER_SIZE], read_buffer[BUFFER_SIZE];

    memset(input_buffer, 0, BUFFER_SIZE);
    memset(read_buffer, 0, BUFFER_SIZE);

    while (1) {
        /* Clear the file descriptor sets */
        FD_ZERO(&write_fds);
        FD_ZERO(&read_fds);

        FD_SET(sock, &write_fds);

        FD_SET(sock, &read_fds);
        FD_SET(STDIN, &read_fds);

        select_fd = select(max_fd, &read_fds, &write_fds, NULL, NULL);

        if (select_fd == -1) {
            perror("Error during select.\n");
        } else if (select_fd) {
            if (FD_ISSET(STDIN, &read_fds)) {
                /* read user input and send the message */
                /* "%*[^\n]%*c" discards all the input after 80 characters */
                fgets(input_buffer, 100, stdin);
                if (strcmp(input_buffer, ":q\n") == 0 || strcmp(input_buffer, ":Q\n") == 0) {
                    status = close(sock);
                    if (status == -1) {
                        perror("Could not close the socket.\n");
                    } else {
                        printf("Goodbye :-)\n");
                        return 0;
                    }
                }
            }
            if (FD_ISSET(sock, &read_fds)) {
                /* read incoming data from socket */
                bytes_received = recvfrom(sock, read_buffer, BUFFER_SIZE, 0, NULL, NULL);
                if (bytes_received == 0) {
                    printf("Other user has disconnected, exiting.\n");
                    break;
                } else if (bytes_received == -1) {
                    perror("Error, lost connection");
                    break;
                }
                
                read_buffer[bytes_received] = '\0';
                printf("%s: %s", prefs.remote_username, read_buffer);
                memset(read_buffer, 0, BUFFER_SIZE);
            }
            if (FD_ISSET(sock, &write_fds)) {
                /* send any available messages */
                if (strlen(input_buffer) > 0) {
                    status = sendto(sock, input_buffer, strlen(input_buffer), 0,
                        (struct sockaddr *)remote_addr, addr_len);
                    if (status == -1) {
                        printf("The message could not be sent :-(\n%s", strerror(errno));
                    }
                    memset(input_buffer, 0, BUFFER_SIZE);
                }
            }
        }

        sleep(1);
    }



    if (prefs.host != NULL) {
        free(prefs.host);
    }
    if (prefs.username != NULL) {
        free(prefs.username);
    }
    if (prefs.remote_username != NULL) {
        free(prefs.remote_username);
    }
    if (remote_addr != NULL) {
        free(remote_addr);
    }

    return 0;
}

struct sockaddr_in* init_server(int sock, struct settings *prefs) {
    struct sockaddr_in *server_addr = malloc(sizeof(struct sockaddr)),
                       *client_addr = malloc(sizeof(struct sockaddr));
    char buffer[BUFFER_SIZE];
    int bytes_received;
    socklen_t addr_len = sizeof(struct sockaddr);

    memset(buffer, 0, BUFFER_SIZE);

    /* AF_INET is for IPv4 */
    server_addr->sin_family = AF_INET;
    /* htons makes sure port number is represented in MSB first */
    server_addr->sin_port = htons(prefs->port);
    server_addr->sin_addr.s_addr = INADDR_ANY;

    memset(&(server_addr->sin_zero), 0, sizeof(server_addr->sin_zero));

    if (bind(sock, (struct sockaddr*)server_addr, addr_len) == -1) {
        perror("Could not bind a socket.\n");
        return NULL;
    }

    printf("UDP server started. Awaiting client connection on port %d.\n", prefs->port);

    while (1) {
        /* Initial message should be the client username */
        bytes_received = recvfrom(sock, buffer, BUFFER_SIZE, 0,
                (struct sockadrr*)client_addr, &addr_len);

        if (bytes_received <= 0) {
            perror("Could not receive handshake data.\n");
            return NULL;
        } else if (bytes_received > 10) {
            perror("Username sent by the client is too long.\n");
        } else {
            sendto(sock, prefs->username, strlen(prefs->username), 0,
                (struct sockaddr*)client_addr, addr_len);
            break;
        }
    }


    /* Save the username client has sent */
    prefs->remote_username = malloc((USERNAME_LEN + 1) * sizeof(char));
    strcpy(prefs->remote_username, buffer);

    printf("Connection estabilished, client address %s:%d, client username: %s\n",
            inet_ntoa(client_addr->sin_addr),
            ntohs(client_addr->sin_port),
            prefs->remote_username);

    return client_addr;
}

struct sockaddr_in* init_client(int sock, struct settings *prefs) {
    struct sockaddr_in *server_addr = malloc(sizeof(struct sockaddr));
    /* resolve the address of the host */
    struct hostent *h = (struct hostent*)gethostbyname(prefs->host);
    char buffer[BUFFER_SIZE];
    int bytes_received;
    unsigned int addr_len = sizeof(struct sockaddr);

    memset(buffer, 0, BUFFER_SIZE);

    if (h == NULL) {
        perror("Could not resolve host name");
        return NULL;
    }
    
    /* AF_INET is for IPv4 */
    server_addr->sin_family = AF_INET;
    /* htons makes sure port number is represented in MSB first */
    server_addr->sin_port = htons(prefs->port);
    server_addr->sin_addr = *((struct in_addr*)h->h_addr_list[0]);

    memset(&(server_addr->sin_zero), 0, sizeof(server_addr->sin_zero));

    /* Send the username to the server */
    bytes_received = sendto(sock, prefs->username, strlen(prefs->username), 0,
            (struct sockaddr*)server_addr, addr_len);
    if (bytes_received == -1) {
        perror("Could not send username to the server.\n");
        return NULL;
    }

    /* Receive server username */
    bytes_received = recvfrom(sock, buffer, BUFFER_SIZE, 0,
                (struct sockaddr*)server_addr, &addr_len);
    
    if (bytes_received == -1) {
        perror("Could not receive the server username.\n");
        return NULL;
    } else if (bytes_received > 10) {
        perror("Received username is too long.\n");
        return NULL;
    }

    buffer[bytes_received] = '\0';
    prefs->remote_username = malloc((USERNAME_LEN + 1) * sizeof(char));
    strcpy(prefs->remote_username, buffer);

    printf("Connected to user %s at %s.\n", prefs->remote_username, prefs->host);
    
    return server_addr;
}

int parse_args(int argc, char *argv[], struct settings *prefs)
{
    int option;

    while ((option = getopt(argc, argv, "p:h:u:")) != -1) {
        switch (option) {
            case 'p': {
                prefs->port = atoi(optarg);
                if (prefs->port <= 0 || prefs->port > 65535) {
                    perror("Invalid port number!\n");
                    return -1;
                }
                break;
            }
            case 'u': {
                if (strlen(optarg) > USERNAME_LEN) {
                    perror("Chosen username is too long!\n");
                    return -1;
                }
                prefs->username = malloc((USERNAME_LEN + 1) *sizeof(char));
                strcpy(prefs->username, optarg);
                break;
            }
            case 'h': {
                if (strlen(optarg) > HOST_LEN) {
                    perror("Chosen host adress is too long!");
                    return -1;
                }
                prefs->host = malloc((HOST_LEN + 1) * sizeof(char));
                strcpy(prefs->host, optarg);
                break;
            }
            default: {
                printf("Usage: utalk [-h host] -p port_number -u username\n");
                return -1;
            }
        }
    }
    return 0;
}
