#include "rps.h"
#include "logic.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

int rpsp_send(int sockfd, const uint8_t *packet, size_t packet_len, const char *dest_ip);

static rpsp_move_t pick(void) { return (rpsp_move_t)((rand() % 3) + 1); }

static const char *name(rpsp_move_t m) {
    switch (m) {
        case MOVE_ROCK: return "ROCK";
        case MOVE_PAPER: return "PAPER";
        case MOVE_SCISSORS: return "SCISSORS";
        default: return "?";
    }
}

static int win(rpsp_move_t a, rpsp_move_t b) {
    return (a == MOVE_ROCK && b == MOVE_SCISSORS) ||
           (a == MOVE_SCISSORS && b == MOVE_PAPER) ||
           (a == MOVE_PAPER && b == MOVE_ROCK);
}

void rpsp_session_init(rpsp_session_t *s, rpsp_role_t role) {
    srand((unsigned)time(NULL));
    s->role = role;
    s->my_move = MOVE_NONE;
    s->done = 0;
}

void rpsp_client_start(rpsp_session_t *s, int fd, const char *ip) {
    s->my_move = pick();
    printf("[rps] I throw %s\n", name(s->my_move));
    uint8_t b[2] = {RPSP_MAGIC, (uint8_t)s->my_move};
    rpsp_send(fd, b, 2, ip);
}

void rpsp_handle_packet(rpsp_session_t *s, int fd, uint8_t *p, size_t len, const char *ip) {
    if (len < 2 || p[0] != RPSP_MAGIC) return;

    uint8_t t = p[1];

    if (t == 0xFD) {
        printf("[rps] DATA: %.*s\n", (int)(len-2), (char*)p+2);
        s->done = 1;
        return;
    }
    if (t < MOVE_ROCK || t > MOVE_SCISSORS) return;

    if (s->role == ROLE_SERVER) {
        rpsp_move_t my = pick();
        printf("[rps] They: %s, Me: %s\n", name((rpsp_move_t)t), name(my));

        if (my == (rpsp_move_t)t) {
            uint8_t r[3] = {RPSP_MAGIC, (uint8_t)my, 0};
            rpsp_send(fd, r, 3, ip);
            printf("[rps] DRAW\n");
            return;
        }

        if (win(my, (rpsp_move_t)t)) {
            uint8_t r[3] = {RPSP_MAGIC, (uint8_t)my, 1};
            rpsp_send(fd, r, 3, ip);
            char *msg = "server wins!";
            size_t n = strlen(msg);
            uint8_t *d = malloc(2+n);
            d[0] = RPSP_MAGIC; d[1] = 0xFD;
            memcpy(d+2, msg, n);
            rpsp_send(fd, d, 2+n, ip);
            free(d);
            printf("[rps] I WIN, sent data\n");
            s->done = 1;
        } else {
            uint8_t r[3] = {RPSP_MAGIC, (uint8_t)my, 2};
            rpsp_send(fd, r, 3, ip);
            printf("[rps] I LOSE, waiting for data...\n");
        }
        return;
    }

    if (len < 3) return;
    int res = p[2];
    printf("[rps] Server: %s\n", name((rpsp_move_t)t));

    if (res == 0) {
        printf("[rps] DRAW, retry\n");
        rpsp_client_start(s, fd, ip);
    } else if (res == 2) {
        char *msg = "client wins!";
        size_t n = strlen(msg);
        uint8_t *d = malloc(2+n);
        d[0] = RPSP_MAGIC; d[1] = 0xFD;
        memcpy(d+2, msg, n);
        rpsp_send(fd, d, 2+n, ip);
        free(d);
        printf("[rps] I WIN, sent data\n");
        s->done = 1;
    } else {
        printf("[rps] I LOSE, waiting for data...\n");
    }
}
