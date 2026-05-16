#include "rps.h"
#include "logic.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define BUF_SIZE 65535

/* from socket.c */
int     rpsp_socket_open  (void);
void    rpsp_socket_close (int sockfd);
ssize_t rpsp_recv         (int sockfd, uint8_t *buffer,
                           size_t buffer_len, char *src_ip,
                           size_t src_ip_len);

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "usage:\n"
                        "  %s server\n"
                        "  %s client <ip>\n", argv[0], argv[0]);
        return 1;
    }

    /* ── role ── */
    rpsp_role_t role;
    const char *dest_ip = NULL;

    if (strcmp(argv[1], "server") == 0) {
        role = ROLE_SERVER;
        printf("[main] SERVER mode\n");

    } else if (strcmp(argv[1], "client") == 0 && argc == 3) {
        role    = ROLE_CLIENT;
        dest_ip = argv[2];
        printf("[main] CLIENT mode → %s\n", dest_ip);

    } else {
        fprintf(stderr, "error: client needs <ip>\n");
        return 1;
    }

    /* ── socket ── */
    int sockfd = rpsp_socket_open();
    if (sockfd < 0) return 1;

    /* ── timeout 5s ── */
    struct timeval tv = { .tv_sec = 5, .tv_usec = 0 };
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    /* ── session ── */
    rpsp_session_t session;
    rpsp_session_init(&session, role);

    /* ── client starts with HELLO ── */
    if (role == ROLE_CLIENT)
        rpsp_client_start(&session, sockfd, dest_ip);

    /* ── event loop ── */
    uint8_t buf[BUF_SIZE];
    char    src_ip[INET_ADDRSTRLEN];

    while (session.state != STATE_DONE &&
           session.state != STATE_DEADLOCK) {

        memset(buf, 0, sizeof(buf));

        ssize_t len = rpsp_recv(sockfd, buf, sizeof(buf),
                                src_ip, sizeof(src_ip));

        /* timeout → DEADLOCK */
        if (len < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            printf("[main] timeout → DEADLOCK\n");
            session.state = STATE_DEADLOCK;
            break;
        }

        /* not our packet → skip */
        if (len == 0) continue;

        if (len < 0) break;

        /* skip IP header */
        int ip_hlen = (buf[0] & 0x0F) * 4;

        rpsp_handle_packet(&session, sockfd,
                           buf + ip_hlen,
                           (size_t)(len - ip_hlen),
                           src_ip);
    }

    /* ── done ── */
    if (session.state == STATE_DEADLOCK)
        printf("[main] ended: DEADLOCK\n");
    else
        printf("[main] ended: OK\n");

    rpsp_socket_close(sockfd);
    return 0;
}