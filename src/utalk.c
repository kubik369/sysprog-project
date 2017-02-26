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

/* select blocking timeout in seconds */
#define TIMEOUT 10

int parse_args(int argc, char *argv[], int *port, char **username,
               char **host);

int main(int argc, char *argv[])
{
    int port = -1;
    char *host = NULL, *username = NULL;

    if (parse_args(argc, argv, &port, &username, &host) == -1) {
        return 1;
    }

    printf("Port number: %d\nUsername: %s\nHost: %s\n", port, username, host);

    if (port == -1 || username == NULL) {
        perror("Port and username are required arguments!\n");
        return 1;
    }

    struct sockaddr_in server_address, client_address;
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    unsigned int addr_len = sizeof(struct sockaddr);
    char input_buffer[BUFFER_SIZE], read_buffer[BUFFER_SIZE];

    memset(input_buffer, 0, BUFFER_SIZE);
    memset(read_buffer, 0, BUFFER_SIZE);

    if (sock == -1) {
        perror("Could not create a socket.\n");
        return 1;
    }

    /* AF_INET is for IPv4 */
    server_address.sin_family = AF_INET;
    client_address.sin_family = AF_INET;
    /* htons makes sure port number is represented in MSB first */
    server_address.sin_port = htons(port);
    memset(&(server_address.sin_zero), 0, sizeof(server_address.sin_zero));

    if (host == NULL) {
        server_address.sin_addr.s_addr = INADDR_ANY;

        if (bind(sock, (struct sockaddr*)&server_address,
                sizeof(struct sockaddr)) == -1)
        {
            perror("Could not bind a socket.\n");
            return 1;
        }
        printf("UDP server started. Awaiting client connection on port %d.\n",
                port);

        int bytes_received = recvfrom(sock, read_buffer, BUFFER_SIZE, 0,
                (struct sockaddr*)&client_address, &addr_len);

        if (bytes_received <= 0) {
            perror("Error during connection, exiting.");
            return 1;
        }
        //inet_ntop(AF_INET, &client_address.sin_addr, from_ip, sizeof(from_ip));
        printf("Connection estabilished, client address %s:%d",
                inet_ntoa(client_address.sin_addr),
                ntohs(client_address.sin_port));
        return 0;
    } else {
        /* resolve the address of the host */
        struct hostent *h = (struct hostent*)gethostbyname(host);

        if (h == NULL) {
            perror("Could not resolve host name");
            return 1;
        }
        server_address.sin_addr = *((struct in_addr*)h->h_addr_list[0]);

        int status = sendto(sock, username, strlen(username), 0,
                (struct sockaddr*)&server_address, sizeof(struct sockaddr));
        if (status == -1) {
            perror("Error during connecting to the server.\n");
            return 1;
        }
        printf("Connection to the host estabilished.\n");
        return 0;
    }

    fflush(stdout);

    fd_set write_fds, read_fds;
    int max_fd = (sock > STDIN) ? sock : STDIN,
        select_fd = 0,
        bytes_received;
    //struct timeval timeout = {TIMEOUT, 0};

    while (1) {
        /* Clear the file descriptor sets */
        FD_ZERO(&write_fds);
        FD_ZERO(&read_fds);

        FD_SET(sock, &write_fds);

        FD_SET(sock, &read_fds);
        FD_SET(STDIN, &read_fds);

        select_fd = select(max_fd + 1, &read_fds, &write_fds, NULL, NULL);

        if (select_fd == -1) {
            perror("Error during select.\n");
        } else if (select_fd) {
            if (FD_ISSET(STDIN, &read_fds)) {
                /* read user input and send the message */
                /* "%*[^\n]%*c" discards all the input after 80 characters */
                scanf("%80s%*[^\n]%*c", input_buffer);
            }
            if (FD_ISSET(sock, &read_fds)) {
                /* read incoming data from socket */
                bytes_received = recvfrom(sock, read_buffer, BUFFER_SIZE,
                                          0, NULL, NULL);
                if (bytes_received == 0) {
                    printf("Other user has disconnected, exiting.\n");
                    break;
                } else if (bytes_received == -1) {
                    perror("Error, lost connection");
                    break;
                } else {
                    read_buffer[bytes_received] = '\0';
                    printf("Message received: %s\n", read_buffer);
                }

            }
            if (FD_ISSET(sock, &write_fds) && strlen(input_buffer) != 0) {
                /* send any available messages */
                sendto(sock, input_buffer, strlen(input_buffer), 0,
                    (struct sockaddr *)&server_address, sizeof(struct sockaddr));
            }
        }
    }

    if (host != NULL) {
        free(host);
    }
    if (username != NULL) {
        free(username);
    }

    return 0;
}

int parse_args(int argc, char *argv[], int *port, char **username,
               char **host)
{
    int option;

    while ((option = getopt(argc, argv, "p:h:u:")) != -1) {
        switch (option) {
            case 'p': {
                *port = atoi(optarg);
                if (*port <= 0 || *port > 65535) {
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
                *username = (char*)malloc((USERNAME_LEN + 1) * sizeof(char));
                strcpy(*username, optarg);
                break;
            }
            case 'h': {
                if (strlen(optarg) > HOST_LEN) {
                    perror("Chosen host adress is too long!");
                    return -1;
                }
                *host = (char*)malloc((HOST_LEN + 1) * sizeof(char));
                strcpy(*host, optarg);
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
