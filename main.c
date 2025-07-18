#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <poll.h>
//#include "Sockets.h"
#include "ff_File.h"

#define BUFFER_SIZE 1024
#define TIMEOUT_MS 2
#define NUM_PACKETS 2
#define SHOW_TICK 1
#define DEFAULT_IP "192.168.0.206"
#define DEFAULT_PORT 50000
#define DEFAULT_MSG "Hello World!"

//Server, array of client_t, at the start of the tick, loop through poll the read for each.

typedef enum {
    STATE_IDLE = 0,
    STATE_CONNECTING,
    STATE_WRITING,
    STATE_READING,
    STATE_DONE,
    STATE_ERROR
} socket_state_t;

typedef enum {
    PROCESS_IDLE = 0,
    PROCESS_WRITING,
} process_state_t;

const char *socket_state_name[] = {
    [STATE_IDLE] = "IDLE",
    [STATE_CONNECTING] = "CONNECTING",
    [STATE_WRITING] = "WRITING",
    [STATE_READING] = "READING",
    [STATE_DONE] = "DONE",
    [STATE_ERROR] = "ERROR"
};

typedef struct {
    int fd;
    struct pollfd pfd;
    socket_state_t sock_state;
}client_t;

typedef struct {
    char *ip_addr;
    int port;
    int pings;
    ff_File_t *file;
}args_t;

client_t g_client_state = {0, 0, STATE_IDLE};
args_t g_args;

int set_nonblocking(const int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}
void update_state(client_t *client, socket_state_t state) {
    printf("State change: %s->%s\n", socket_state_name[client->sock_state], socket_state_name[state]);
    client->sock_state = state;
}

void handle_args(const int argc, char **argv) {
    g_args.file = malloc(sizeof(ff_File_t));
    g_args.file->file_data = malloc(sizeof(char) * BUFFER_SIZE);
    g_args.ip_addr = DEFAULT_IP;
    g_args.port = DEFAULT_PORT;
    g_args.file->file_data = DEFAULT_MSG;
    g_args.file->file_size = (int)strlen(g_args.file->file_data);
    g_args.pings = NUM_PACKETS;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-ip") == 0) {
            g_args.ip_addr = argv[++i];
            continue;
        }
        if (strcmp(argv[i], "-port") == 0) {
            g_args.port = (int)strtol(argv[++i], NULL, 10);
            continue;
        }
        if (strcmp(argv[i], "-file") == 0) {
            ff_File_t *file_date = readFile(argv[++i]);
            if (file_date) {
                g_args.file = file_date;
                continue;
            }
        }
        if (strcmp(argv[i], "-pings") == 0) {
            g_args.pings = (int)strtol(argv[++i], NULL, 10);
            continue;
        }
    }
    if (SHOW_TICK) {
        printf("FINAL ARGS::\n"
               "IP: %s\n"
               "PORT: %d\n"
               "PINGS: %d\n"
               "File_Size: %d\n",
               g_args.ip_addr,
               g_args.port,
               g_args.pings,
               g_args.file->file_size);
    }
}


int main(const int argc, char **argv) {
    struct sockaddr_in server_addr;
    int pong = 1;
    char buffer[BUFFER_SIZE] = {0};
    int connect_started = 0;
    int bytes_wrote = 0;
    handle_args(argc, argv);

    printf("Server ip: %s\n", g_args.ip_addr);
    printf("Server port: %d\n\n", g_args.port);

    // Create socket
    g_client_state.fd = socket(AF_INET, SOCK_STREAM, 0);
    if (g_client_state.fd < 0) {
        perror("socket");
        return EXIT_FAILURE;
    }

    // Set non-blocking
    if (set_nonblocking(g_client_state.fd) < 0) {
        perror("fcntl");
        close(g_client_state.fd);
        return EXIT_FAILURE;
    }

    // Setup server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(g_args.port);
    if (inet_pton(AF_INET, g_args.ip_addr, &server_addr.sin_addr) < 1) {
        perror("inet_pton");
        exit(EXIT_FAILURE);
    }

    // Update state to start machine
    g_client_state.pfd.fd = g_client_state.fd;
    update_state(&g_client_state, STATE_CONNECTING);
    while (g_client_state.sock_state != STATE_DONE && g_client_state.sock_state != STATE_ERROR) {
        if (SHOW_TICK) {
            printf("Tick...\n");
        }
        switch (g_client_state.sock_state) {
            case STATE_CONNECTING:
                if (!connect_started) {
                    const int ret = connect(g_client_state.fd, (struct sockaddr *)&server_addr, sizeof(server_addr));
                    if (ret < 0) {
                        if (errno != EINPROGRESS) {
                            perror("connect");
                            // g_client_state = STATE_ERROR;
                            update_state(&g_client_state, STATE_ERROR);
                            break;
                        }
                        // EINPROGRESS is OK
                    }
                    connect_started = 1;
                }
                g_client_state.pfd.events = POLLOUT;
                break;

            case STATE_WRITING:
                g_client_state.pfd.events = POLLOUT;
                break;

            case STATE_READING:
                g_client_state.pfd.events = POLLIN;
                break;

            default:
                g_client_state.pfd.events = 0;
                break;
        }

        // Poll socket
        const int ret = poll(&g_client_state.pfd, 1, TIMEOUT_MS);
        if (ret < 0) {
            perror("poll");
            update_state(&g_client_state, STATE_ERROR);
            break;
        }else if (ret == 0) {
            // Timeout: no events, but engine stays alive
            // You can add timer handling or state keep-alive logic here
            // printf("Poll timeout, no events. Continuing...\n");
            continue;  // just loop again
        }

        // Check for error events
        if (g_client_state.pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) {
            fprintf(stderr, "Socket error or hangup\n");
            // g_client_state = STATE_ERROR;
            update_state(&g_client_state, STATE_ERROR);
            break;
        }

        // Handle socket event
        switch (g_client_state.sock_state) {
            case STATE_CONNECTING: {
                if (g_client_state.pfd.revents & POLLOUT) {
                    int err = 0;
                    socklen_t len = sizeof(err);
                    if (getsockopt(g_client_state.fd, SOL_SOCKET, SO_ERROR, &err, &len) < 0 || err != 0) {
                        fprintf(stderr, "Connect failed: %s\n", strerror(err));
                        // g_client_state = STATE_ERROR;
                        update_state(&g_client_state, STATE_ERROR);
                    } else {
                        // g_client_state = STATE_WRITING;
                        update_state(&g_client_state, STATE_WRITING);
                    }
                }
                break;
            }

            case STATE_WRITING: {
                if (g_client_state.pfd.revents & POLLOUT) {
                    printf("Ping: %d\n", pong);
                    printf("Writing: %s\n", "...");
                    ssize_t sent = send(g_client_state.fd, g_args.file->file_data + bytes_wrote, g_args.file->file_size - bytes_wrote, MSG_DONTWAIT);
                    if (sent < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
                        perror("send");
                        // g_client_state = STATE_ERROR;
                        update_state(&g_client_state, STATE_ERROR);
                    } else {
                        printf("Bytes wrote: %zu\n", sent);
                        bytes_wrote += (int)sent;
                        if (bytes_wrote < g_args.file->file_size) {
                            pong--;
                        }
                        else {
                            bytes_wrote = 0;
                        }
                        update_state(&g_client_state, STATE_READING);
                    }
                }
                break;
            }

            case STATE_READING: {
                if (g_client_state.pfd.revents & POLLIN) {
                    ssize_t n = recv(g_client_state.fd, buffer, BUFFER_SIZE - 1, MSG_DONTWAIT);
                    if (n > 0) {
                        buffer[(int)n] = '\0';
                        printf("Pong: %d\n", pong);
                        printf("Bytes read: %zu\n", n);
                        printf("Received: %s\n", buffer);
                        // g_client_state = STATE_DONE;
                        if (pong < g_args.pings) {
                            pong++;
                            update_state(&g_client_state, STATE_WRITING);
                        } else {
                            update_state(&g_client_state, STATE_DONE);
                        }
                    } else if (n == 0) {
                        printf("Server closed connection.\n");
                        // g_client_state = STATE_DONE;
                        update_state(&g_client_state, STATE_DONE);
                    } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
                        perror("recv");
                        // g_client_state = STATE_ERROR;
                        update_state(&g_client_state, STATE_ERROR);
                    }
                }
                break;
            }

            default:
                break;
        }
    }
    // Clean up
    close(g_client_state.fd);
    free(g_args.file);
    printf("Client exited with state: %s\n", g_client_state.sock_state == STATE_DONE ? "DONE" : "ERROR");
    return (g_client_state.sock_state == STATE_DONE) ? EXIT_SUCCESS : EXIT_FAILURE;
}
