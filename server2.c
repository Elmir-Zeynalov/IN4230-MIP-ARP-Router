#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/epoll.h>

void client(int epoll_fd, int sock_fd) {
    struct epoll_event events[1];
    char buffer[256];

    // Wait for events
    while (1) {
        int num_events = epoll_wait(epoll_fd, events, 1, -1);
        if (num_events < 0) {
            perror("epoll_wait");
            exit(EXIT_FAILURE);
        }

        for (int i = 0; i < num_events; i++) {
            if (events[i].events & EPOLLIN) {
                memset(buffer, 0, sizeof(buffer));
                int num_bytes = recv(sock_fd, buffer, sizeof(buffer) - 1, 0);
                if (num_bytes > 0) {
                    printf("Received: %s\n", buffer);
                } else if (num_bytes == 0) {
                    printf("Server closed the connection.\n");
                    return;
                } else {
                    perror("recv");
                    exit(EXIT_FAILURE);
                }
            }
        }
    }
}

int main(int argc, char *argv[]) {
    char *socket_path = argv[1];
    int sock_fd, epoll_fd;

    // Create a Unix domain socket
    sock_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock_fd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // Set up the server address
    struct sockaddr_un server_addr;
    memset(&server_addr, 0, sizeof(struct sockaddr_un));
    server_addr.sun_family = AF_UNIX;
    strncpy(server_addr.sun_path, socket_path, sizeof(server_addr.sun_path) - 1);

    // Connect to the server
    if (connect(sock_fd, (struct sockaddr *)&server_addr, sizeof(struct sockaddr_un)) < 0) {
        perror("connect");
        close(sock_fd);
        exit(EXIT_FAILURE);
    }

    // Create an epoll instance
    epoll_fd = epoll_create1(0);
    if (epoll_fd < 0) {
        perror("epoll_create1");
        close(sock_fd);
        exit(EXIT_FAILURE);
    }

    // Add the socket to the epoll instance
    struct epoll_event event;
    event.events = EPOLLIN;
    event.data.fd = sock_fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, sock_fd, &event) < 0) {
        perror("epoll_ctl");
        close(sock_fd);
        close(epoll_fd);
        exit(EXIT_FAILURE);
    }

    // Send a message to the server
    //char message[] = "Hello, server!";
    //send(sock_fd, message, strlen(message), 0);

    // Start waiting for events
    client(epoll_fd, sock_fd);

    // Clean up
    close(sock_fd);
    close(epoll_fd);

    return 0;
}

