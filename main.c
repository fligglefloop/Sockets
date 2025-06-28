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

#define BUFFER_SIZE 1024
#define TIMEOUT_MS 2
#define NUM_PACKETS 2

typedef enum {
    STATE_IDLE = 0,
    STATE_CONNECTING,
    STATE_WRITING,
    STATE_READING,
    STATE_DONE,
    STATE_ERROR
} socket_state_t;

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
    socket_state_t state;
}client_t;

client_t g_socket_state = {0, 0, STATE_IDLE};

int set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}
void update_state(client_t *client, socket_state_t state) {
    printf("State change: %s->%s\n", socket_state_name[client->state], socket_state_name[state]);
    client->state = state;
}

int main(int argc, char **argv) {
    char *server_ip = "192.168.0.206";
    int server_port = 50000;
    struct sockaddr_in server_addr;
    int pong = 1;
    int base_args = 1;
    char buffer[BUFFER_SIZE] = {0};
    const char *msg = "KEGWEL8,nZMj}T7h3b0!rhS?ur[h7.vW*]7hpvb7{&56nrZ]EZjpTc0))m}$P8L}u1(ZA!c}x!+}X.iqbndPgTbvuLQM,a]3Q&V&wcA0v$PvE*QP1M!npYKNr8:c4]/L/wwLqH/;bytq.M!uUh9Q{2ka}{wFqx-2dPJHV0);,Cfi.qmLBHQq)aT)J5cz]S[*HCVgzA,K)dBAHk2$nCnpqEUYjr&wat:*[Kn1hHP(ad#w5LYr{3DU[)cM:3dAh8e$U-JzXtGC%0dS,d;QrQ-x*1b3YwVYxggSTnaG&Zt*3G0q%nx%;hmK7fkadNp?rc,jY+Kk-}/e/WA/tbjNcR/bNTA}N5J8*Fi}pAgF;NXx4H.+t8[@Q{9(r@F0-C&p2c(A/7B&bLXfwPnc9[XGB&9v$)jAPEbJX5:G]%=Kcep.*e?_8}XW%gQ;z3A?0wF([@*TnuDXJ5M);Va$?uihYQTGuvXLd8K=mD-18-#wR-iT)GQu9TvjhAY-DX!79n4:kwX(H4/_kg4Z[xfqRnJeh!LGqXr,Ygtkq78{vWTueKzynRrdt8@SCBR$E$5fPy9=D:t+#[N2!f2x4mi{9AN@_v%{)2XiH&;Y{Rpn:jx?[U47@ku4TS(qchFEnJKx#dyhRJ4K.P%i&igmKGFiGMErwz=U3RM8SF_Kh!E%A$=9BZkF$NFfQ93;k4.f_wjLkh({%AqJg@p5-tQ9rCx!&Z&Q@L6W,_.&)%D+:hP(8!S*xmdhmU9tFtA@7c8Y9:]/nNaE9V}+Bqq11G#Ci,*kL[Xe:!Ngb,LyXSnmZVz#)iWPjB?b9LyREt$w/wk-FNZm/T&JZ.bDKjHQ.dQ:nw#}$?C,$G(Z:ak:F8B0)&i@(n9vRcpG:CS]Ewix)GkL-A+?M(W%0iH&QbHhD}aS5,FLy3zFdqf64B_}di!fydzw]K{0,rZ/Dyv@N.R*yHuc5kz&+rk].-PrDN_FFU38B&;*Sq60HBmC_@?q7pNLUq;rS:p@S_]nm]P#AfgBp,T*t#{xy8F:z3W:S?@UrzZz&6}fa,w]HXTB.iL3z1n11+2?e]wDZzfH,UDFp@$U;M,-6GtZ1217:c1.wz4(*_neZ}v-#4mAAMcFAPP=9pG/Q+@/xzhZX&/S-$vL0)g4m$kgCcQ%$_1]9*RbqFQi3]Bcta5)68eE[6,ADZ[Lzik=Q0HAr{pmat:)?a&nZJ6x{F#Ldv)=2b)Sp*M*&0Mt62:@SgT;[x2GZQ)B-_Hyn@]GT{8RA;X]@#NPJ6j0;MQ;tDbY{{ZmT0xjbFq;fk$WcB,uMvu&0.+Mmx:RZ{qQ-[8B72VS#gu4vN.f*Jn)vZ80L)MeK*@Mi&F*q01DpYC6(k69f9zL#0dkiTj&]_v&TCzpVQz[$xxycH*i8GR$cxM5Ki47xE$j";
    ssize_t msg_len = strlen(msg);
    int connect_started = 0;
    int num_packets = NUM_PACKETS;

    // Handle cli args
    if (argc > base_args) {
        server_ip = argv[1];
        server_port = atoi(argv[2]);
        num_packets = atoi(argv[3]);
    }

    printf("Server ip: %s\n", server_ip);
    printf("Server port: %d\n\n", server_port);

    // Create socket
    g_socket_state.fd = socket(AF_INET, SOCK_STREAM, 0);
    if (g_socket_state.fd < 0) {
        perror("socket");
        return EXIT_FAILURE;
    }

    // Set non-blocking
    if (set_nonblocking(g_socket_state.fd) < 0) {
        perror("fcntl");
        close(g_socket_state.fd);
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
    g_socket_state.pfd.fd = g_socket_state.fd;
    update_state(&g_socket_state, STATE_CONNECTING);
    while (g_socket_state.state != STATE_DONE && g_socket_state.state != STATE_ERROR) {
        switch (g_socket_state.state) {
            case STATE_CONNECTING:
                if (!connect_started) {
                    int ret = connect(g_socket_state.fd, (struct sockaddr *)&server_addr, sizeof(server_addr));
                    if (ret < 0) {
                        if (errno != EINPROGRESS) {
                            perror("connect");
                            // g_client_state = STATE_ERROR;
                            update_state(&g_socket_state, STATE_ERROR);
                            break;
                        }
                        // EINPROGRESS is OK
                    }
                    connect_started = 1;
                }
                g_socket_state.pfd.events = POLLOUT;
                break;

            case STATE_WRITING:
                g_socket_state.pfd.events = POLLOUT;
                break;

            case STATE_READING:
                g_socket_state.pfd.events = POLLIN;
                break;

            default:
                g_socket_state.pfd.events = 0;
                break;
        }

        // Poll socket
        const int ret = poll(&g_socket_state.pfd, 1, TIMEOUT_MS);
        if (ret < 0) {
            perror("poll");
            update_state(&g_socket_state, STATE_ERROR);
            break;
        }else if (ret == 0) {
            // Timeout: no events, but engine stays alive
            // You can add timer handling or state keep-alive logic here
            // printf("Poll timeout, no events. Continuing...\n");
            continue;  // just loop again
        }

        // Check for error events
        if (g_socket_state.pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) {
            fprintf(stderr, "Socket error or hangup\n");
            // g_client_state = STATE_ERROR;
            update_state(&g_socket_state, STATE_ERROR);
            break;
        }

        // Handle socket event
        switch (g_socket_state.state) {
            case STATE_CONNECTING: {
                if (g_socket_state.pfd.revents & POLLOUT) {
                    int err = 0;
                    socklen_t len = sizeof(err);
                    if (getsockopt(g_socket_state.fd, SOL_SOCKET, SO_ERROR, &err, &len) < 0 || err != 0) {
                        fprintf(stderr, "Connect failed: %s\n", strerror(err));
                        // g_client_state = STATE_ERROR;
                        update_state(&g_socket_state, STATE_ERROR);
                    } else {
                        // g_client_state = STATE_WRITING;
                        update_state(&g_socket_state, STATE_WRITING);
                    }
                }
                break;
            }

            case STATE_WRITING: {
                if (g_socket_state.pfd.revents & POLLOUT) {
                    printf("Ping: %d\n", pong);
                    printf("Writing: %s\n", msg);
                    ssize_t sent = send(g_socket_state.fd, msg, msg_len, 0);
                    if (sent < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
                        perror("send");
                        // g_client_state = STATE_ERROR;
                        update_state(&g_socket_state, STATE_ERROR);
                    } else {
                        printf("Bytes wrote: %zu\n", sent);
                        // g_client_state = STATE_READING;
                        update_state(&g_socket_state, STATE_READING);
                    }
                }
                break;
            }

            case STATE_READING: {
                if (g_socket_state.pfd.revents & POLLIN) {
                    ssize_t n = recv(g_socket_state.fd, buffer, BUFFER_SIZE - 1, 0);
                    if (n > 0) {
                        buffer[(int)n] = '\0';
                        printf("Pong: %d\n", pong);
                        printf("Bytes read: %zu\n", n);
                        printf("Received: %s\n", buffer);
                        // g_client_state = STATE_DONE;
                        if (pong < num_packets) {
                            pong++;
                            update_state(&g_socket_state, STATE_WRITING);
                        } else {
                            update_state(&g_socket_state, STATE_DONE);
                        }
                    } else if (n == 0) {
                        printf("Server closed connection.\n");
                        // g_client_state = STATE_DONE;
                        update_state(&g_socket_state, STATE_DONE);
                    } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
                        perror("recv");
                        // g_client_state = STATE_ERROR;
                        update_state(&g_socket_state, STATE_ERROR);
                    }
                }
                break;
            }

            default:
                break;
        }
    }
    // Clean up
    close(g_socket_state.fd);
    printf("Client exited with state: %s\n", g_socket_state.state == STATE_DONE ? "DONE" : "ERROR");
    return (g_socket_state.state == STATE_DONE) ? EXIT_SUCCESS : EXIT_FAILURE;
}
