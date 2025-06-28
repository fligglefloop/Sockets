// tcp_client_poll_fsm_connect.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <poll.h>

#define BUFFER_SIZE 1024
#define TIMEOUT_MS 2

typedef enum {
    STATE_IDLE = 0,
    STATE_CONNECTING,
    STATE_WRITING,
    STATE_READING,
    STATE_DONE,
    STATE_ERROR
} client_state_t;

char *state_name[] = {
    [STATE_IDLE] = "IDLE",
    [STATE_CONNECTING] = "CONNECTING",
    [STATE_WRITING] = "WRITING",
    [STATE_READING] = "READING",
    [STATE_DONE] = "DONE",
    [STATE_ERROR] = "ERROR"
};

client_state_t g_client_state = STATE_IDLE;

int set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}
void update_state(client_state_t state) {
    g_client_state = state;
    printf("State change: %s\n", state_name[state]);
}

int main(int argc, char **argv) {
    char *server_ip = "192.168.0.206";
    int server_port = 50000;
    int sockfd;
    struct sockaddr_in server_addr;
    struct pollfd pfd;
    int pong = 1;
    int base_args = 1;
    char buffer[BUFFER_SIZE] = {0};
    const char *msg = "Hello, Server!";
    ssize_t msg_len = strlen(msg);
    int connect_started = 0;
    int num_packets = 20;

    // Handle cli args
    if (argc > base_args) {
        server_ip = argv[1];
        server_port = atoi(argv[2]);
        num_packets = atoi(argv[3]);
    }

    printf("Server ip: %s\n", server_ip);
    printf("Server port: %d\n\n", server_port);

    // Create socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket");
        return EXIT_FAILURE;
    }

    // Set non-blocking
    if (set_nonblocking(sockfd) < 0) {
        perror("fcntl");
        close(sockfd);
        return EXIT_FAILURE;
    }

    // Setup server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) < 1) {
        perror("inet_pton");
        exit(EXIT_FAILURE);
    }

    // Update state to start machine
    pfd.fd = sockfd;
    update_state(STATE_CONNECTING);
    while (g_client_state != STATE_DONE && g_client_state != STATE_ERROR) {
        switch (g_client_state) {
            case STATE_CONNECTING:
                if (!connect_started) {
                    int ret = connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr));
                    if (ret < 0) {
                        if (errno != EINPROGRESS) {
                            perror("connect");
                            // g_client_state = STATE_ERROR;
                            update_state(STATE_ERROR);
                            break;
                        }
                        // EINPROGRESS is OK
                    }
                    connect_started = 1;
                }
                pfd.events = POLLOUT;
                break;

            case STATE_WRITING:
                pfd.events = POLLOUT;
                break;

            case STATE_READING:
                pfd.events = POLLIN;
                break;

            default:
                pfd.events = 0;
                break;
        }

        // Poll socket
        const int ret = poll(&pfd, 1, TIMEOUT_MS);
        if (ret < 0) {
            perror("poll");
            update_state(STATE_ERROR);
            break;
        }else if (ret == 0) {
            // Timeout: no events, but engine stays alive
            // You can add timer handling or state keep-alive logic here
            // printf("Poll timeout, no events. Continuing...\n");
            continue;  // just loop again
        }

        // Check for error events
        if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) {
            fprintf(stderr, "Socket error or hangup\n");
            // g_client_state = STATE_ERROR;
            update_state(STATE_ERROR);
            break;
        }

        // Handle socket event
        switch (g_client_state) {
            case STATE_CONNECTING: {
                if (pfd.revents & POLLOUT) {
                    int err = 0;
                    socklen_t len = sizeof(err);
                    if (getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &err, &len) < 0 || err != 0) {
                        fprintf(stderr, "Connect failed: %s\n", strerror(err));
                        // g_client_state = STATE_ERROR;
                        update_state(STATE_ERROR);
                    } else {
                        // g_client_state = STATE_WRITING;
                        update_state(STATE_WRITING);
                    }
                }
                break;
            }

            case STATE_WRITING: {
                if (pfd.revents & POLLOUT) {
                    printf("Ping: %d\n", pong);
                    printf("Writing: %s\n", msg);
                    ssize_t sent = send(sockfd, msg, msg_len, 0);
                    if (sent < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
                        perror("send");
                        // g_client_state = STATE_ERROR;
                        update_state(STATE_ERROR);
                    } else {
                        printf("Bytes Wrote: %zu\n", sent);
                        // g_client_state = STATE_READING;
                        update_state(STATE_READING);
                    }
                }
                break;
            }

            case STATE_READING: {
                if (pfd.revents & POLLIN) {
                    ssize_t n = recv(sockfd, buffer, BUFFER_SIZE - 1, 0);
                    if (n > 0) {
                        buffer[(int)n] = '\0';
                        printf("Pong: %d\n", pong);
                        printf("Bytes Read: %zu\n", n);
                        printf("Received: %s\n", buffer);
                        // g_client_state = STATE_DONE;
                        if (pong < num_packets) {
                            pong++;
                            update_state(STATE_WRITING);
                        } else {
                            update_state(STATE_DONE);
                        }
                    } else if (n == 0) {
                        printf("Server closed connection.\n");
                        // g_client_state = STATE_DONE;
                        update_state(STATE_DONE);
                    } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
                        perror("recv");
                        // g_client_state = STATE_ERROR;
                        update_state(STATE_ERROR);
                    }
                }
                break;
            }

            default:
                break;
        }
    }
    // Clean up
    close(sockfd);
    printf("Client exited with state: %s\n", g_client_state == STATE_DONE ? "DONE" : "ERROR");
    return (g_client_state == STATE_DONE) ? EXIT_SUCCESS : EXIT_FAILURE;
}
