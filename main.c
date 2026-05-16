#include "rps.h"
#include "logic.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define BUF 65535

int rpsp_socket_open(void);
void rpsp_socket_close(int fd);
ssize_t rpsp_recv(int fd, uint8_t *buf, size_t n, char *ip, size_t iplen);

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: %s server | client <ip>\n", argv[0]);
        return 1;
    }

    rpsp_role_t role;
    const char *dest = NULL;

    if (!strcmp(argv[1], "server")) role = ROLE_SERVER;
    else if (argc == 3 && !strcmp(argv[1], "client")) { role = ROLE_CLIENT; dest = argv[2]; }
    else { fprintf(stderr, "need: server or client <ip>\n"); return 1; }

    int fd = rpsp_socket_open();
    if (fd < 0) return 1;

    struct timeval tv = {5, 0};
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    rpsp_session_t s;
    rpsp_session_init(&s, role);

    if (role == ROLE_CLIENT) rpsp_client_start(&s, fd, dest);

    uint8_t buf[BUF];
    char ip[INET_ADDRSTRLEN];

    while (!s.done) {
        ssize_t n = rpsp_recv(fd, buf, sizeof(buf), ip, sizeof(ip));
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) { printf("[main] timeout\n"); break; }
            break;
        }
        if (n == 0) continue;
        int hlen = (buf[0] & 0x0F) * 4;
        rpsp_handle_packet(&s, fd, buf + hlen, (size_t)(n - hlen), ip);
    }

    rpsp_socket_close(fd);
    printf("[main] done\n");
    return 0;
}
