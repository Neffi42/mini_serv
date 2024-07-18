#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>
#include <arpa/inet.h>

typedef struct s_client {
    int id;
    int fd;
    char msg[100000];
} t_client;

t_client    clients[1024];
fd_set      read_set, write_set;
int         maxfd = 0, gid = 0;
char        write_buf[150000], read_buf[150000];

void err(char *message) {
    if (!message)
        write(2, "Fatal error", strlen("Fatal error"));
    else
        write(2, message, strlen(message));
    write(2, "\n", 1);
    exit(1);
}

void    send_to_all(int except)
{
    for (int fd = 0; fd <= maxfd; ++fd) {
        if  (FD_ISSET(fd, &write_set) && fd != except)
            if (send(fd, write_buf, strlen(write_buf), 0) == -1)
                err(NULL);
    }
}

int main(int argc, char** argv) {
    if (argc != 2)
        err("Wrong number of arguments");

    int port = atoi(argv[1]);
    if (port <= 0)
        err(NULL);

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1)
        err(NULL);

    maxfd = sockfd;
    bzero(clients, sizeof(clients));

    struct sockaddr_in servaddr;
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(2130706433);
    servaddr.sin_port = htons(port);

    if (bind(sockfd, (const struct sockaddr*)&servaddr, sizeof(servaddr)) == -1
        || listen(sockfd, 10) == -1)
        err(NULL);

    while (1) {
        FD_ZERO(&read_set);
        FD_ZERO(&write_set);
        FD_SET(sockfd, &read_set);
        for (int i = 0; i <= maxfd; i++) {
            if (clients[i].fd != 0) {
                FD_SET(clients[i].fd, &read_set);
                FD_SET(clients[i].fd, &write_set);
            }
        }

        if (select(maxfd + 1, &read_set, &write_set, NULL, 0) == -1)
            continue;

        for (int fd = 0; fd <= maxfd; ++fd) {
            if (FD_ISSET(fd, &read_set))
            {
                if (fd == sockfd)
                {
                    int clientfd = accept(sockfd, NULL, NULL);
                    if (clientfd == -1)
                        continue;

                    if (clientfd > maxfd)
                        maxfd = clientfd;

                    clients[clientfd].id = gid++;
                    clients[clientfd].fd = clientfd;
                    sprintf(write_buf, "server: client %d just arrived\n", clients[clientfd].id);
                    send_to_all(clientfd);
                }
                else
                {
                    int n = recv(fd, read_buf, sizeof(read_buf), 0);
                    if (n <= 0)
                    {
                        clients[fd].fd = 0;
                        sprintf(write_buf, "server: client %d just left\n", clients[fd].id);
                        send_to_all(fd);
                        close(fd);
                    }
                    else
                    {
                        for (int i = 0, j = 0; i < n; i++, j++)
                        {
                            clients[fd].msg[j] = read_buf[i];
                            if (clients[fd].msg[j] == '\n')
                            {
                                clients[fd].msg[j] = '\0';
                                sprintf(write_buf, "client %d: %s\n", clients[fd].id, clients[fd].msg);
                                send_to_all(fd);
                                bzero(clients[fd].msg, strlen(clients[fd].msg));
                                j = -1;
                            }
                        }
                        bzero(read_buf, sizeof(read_buf));
                    }
                }
            }
        }
    }

    return 0;
}
