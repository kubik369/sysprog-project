#include "network.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>

struct sockaddr_in* init_server(int sock, struct settings *prefs) {
    struct sockaddr_in *server_addr = malloc(sizeof(struct sockaddr_in)),
                       *client_addr = malloc(sizeof(struct sockaddr_in));
    char buffer[BUFFER_SIZE + 1];
    int bytes_received;
    socklen_t addr_len = sizeof(struct sockaddr);

    memset(buffer, 0, BUFFER_SIZE + 1);

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

    printf("UDP server started. Awaiting client connection on port %d.\n",
            prefs->port);

    /* Initial message should be the client username */
    bytes_received = recvfrom(sock, buffer, BUFFER_SIZE, 0,
            (struct sockadrr*)client_addr, &addr_len);

    if (bytes_received <= 0) {
        perror("Could not receive handshake data.\n");
        return NULL;
    } else if (bytes_received > 10) {
        perror("Username sent by the client is too long.\n");
        return NULL;
    } 
    
    sendto(sock, prefs->username, strlen(prefs->username), 0,
        (struct sockaddr*)client_addr, addr_len);

    /* Save the username client has sent */
    prefs->remote_username = malloc((USERNAME_LEN + 1) * sizeof(char));
    strcpy(prefs->remote_username, buffer);

    printf(
        "Connection estabilished, client address %s:%d, client username: %s\n",
        inet_ntoa(client_addr->sin_addr),
        ntohs(client_addr->sin_port),
        prefs->remote_username
    );

    prefs->host = malloc((HOST_LEN + 1) * sizeof(char));
    strcpy(prefs->host, inet_ntoa(client_addr->sin_addr));

    free(server_addr);
    return client_addr;
}

struct sockaddr_in* init_client(int sock, struct settings *prefs) {
    struct sockaddr_in *server_addr = malloc(sizeof(struct sockaddr));
    /* resolve the address of the host */
    struct hostent *h = (struct hostent*)gethostbyname(prefs->host);
    char buffer[BUFFER_SIZE + 1];
    int bytes_received;
    unsigned int addr_len = sizeof(struct sockaddr);

    memset(buffer, 0, BUFFER_SIZE + 1);

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
    
    int retry_cnt = 0;
    while(1) {
        if (retry_cnt >= 10) {
            perror("Could not establish connection with the server.\n");
            return NULL;
        }
        retry_cnt++;
        printf("Establishing connection, attempt #%d.\n", retry_cnt);

        /* Send the username to the server */
        bytes_received = sendto(sock, prefs->username, strlen(prefs->username),
                                0, (struct sockaddr*)server_addr, addr_len);
        if (bytes_received == -1) {
            perror("Could not send username to the server.\n");
            continue;
        }

        /* Receive server username */
        bytes_received = recvfrom(sock, buffer, BUFFER_SIZE, 0,
                    (struct sockaddr*)server_addr, &addr_len);
        
        if (bytes_received == -1) {
            continue;
        } else if (bytes_received > 10 || bytes_received == 0) {
            perror("Received username is either too long or too short.\n");
            return NULL;
        }
        break;
    }

    buffer[bytes_received] = '\0';
    prefs->remote_username = malloc((USERNAME_LEN + 1) * sizeof(char));
    strcpy(prefs->remote_username, buffer);

    printf("Connected to user %s at %s.\n", prefs->remote_username,
            prefs->host);
    
    return server_addr;
}
