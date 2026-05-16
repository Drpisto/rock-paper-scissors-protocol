#include "rps.h"
#include "logic.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
 
/* forward declaration من socket.c */
int rpsp_send(int sockfd, const uint8_t *packet,
              size_t packet_len, const char *dest_ip);
 
/* ─────────────────────────────────────────────
   Session
───────────────────────────────────────────── */
 
void rpsp_session_init(rpsp_session_t *s, rpsp_role_t role) {
    srand((unsigned)time(NULL));
    s->state      = STATE_IDLE;
    s->role       = role;
    s->session_id = (uint16_t)(time(NULL) ^ (uint16_t)rand());
    s->my_move    = MOVE_NONE;
    s->their_move = MOVE_NONE;
    s->round      = 0;
    s->draws      = 0;
}
 
/* ─────────────────────────────────────────────
   RPS Logic
───────────────────────────────────────────── */
 
static rpsp_move_t pick_move(void) {
    return (rpsp_move_t)((rand() % 3) + 1);
}
 
static rpsp_result_t determine_result(rpsp_move_t mine, rpsp_move_t theirs) {
    if (mine == theirs) return RESULT_DRAW;
    if ((mine == MOVE_ROCK     && theirs == MOVE_SCISSORS) ||
        (mine == MOVE_SCISSORS && theirs == MOVE_PAPER)    ||
        (mine == MOVE_PAPER    && theirs == MOVE_ROCK))
        return RESULT_WIN;
    return RESULT_LOSE;
}
 
/* ─────────────────────────────────────────────
   Send helper — يستخدم rps_header من rps.h
───────────────────────────────────────────── */
 
static void send_type(int sockfd, rpsp_session_t *s,
                      rpsp_type_t type, rpsp_move_t move,
                      const char *dest_ip) {
    uint8_t buf[RPSP_HEADER_SIZE];
 
    buf[0] = RPSP_MAGIC;
    buf[1] = (uint8_t)type;
    buf[2] = RPSP_SET_MOVE_ROUND((uint8_t)move, s->round);
    buf[3] = (uint8_t)(s->session_id >> 8);
    buf[4] = (uint8_t)(s->session_id & 0xFF);
    buf[5] = 0x00;
    buf[6] = 0x00;
 
    /* checksum على الـ RPSP header فقط */
    uint32_t sum = 0;
    for (int i = 0; i < RPSP_HEADER_SIZE; i++) sum += buf[i];
    while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
    uint16_t cksum = (uint16_t)(~sum);
    buf[5] = (uint8_t)(cksum >> 8);
    buf[6] = (uint8_t)(cksum & 0xFF);
 
    rpsp_send(sockfd, buf, RPSP_HEADER_SIZE, dest_ip);
}
 
/* ─────────────────────────────────────────────
   Handlers
───────────────────────────────────────────── */
 
static void on_hello(rpsp_session_t *s, int sockfd, const char *src_ip) {
    if (s->state != STATE_IDLE) return;
    printf("[logic] HELLO ← %s\n", src_ip);
    send_type(sockfd, s, RPSP_ACCEPT, MOVE_NONE, src_ip);
    s->state = STATE_WAITING_MOVE;
}
 
static void on_accept(rpsp_session_t *s, int sockfd, const char *src_ip) {
    if (s->state != STATE_HANDSHAKE) return;
    printf("[logic] ACCEPT\n");
    s->my_move = pick_move();
    send_type(sockfd, s, RPSP_MOVE, s->my_move, src_ip);
    s->state = STATE_WAITING_RESULT;
}
 
static void on_move(rpsp_session_t *s, int sockfd,
                    rpsp_move_t their_move, const char *src_ip) {
    if (s->state != STATE_WAITING_MOVE) return;
 
    s->their_move = their_move;
    s->my_move    = pick_move();
    s->round++;
 
    printf("[logic] حركتهم=%d حركتي=%d\n",
           (int)s->their_move, (int)s->my_move);
 
    send_type(sockfd, s, RPSP_MOVE, s->my_move, src_ip);
 
    rpsp_result_t result = determine_result(s->my_move, s->their_move);
 
    if (result == RESULT_DRAW) {
        s->draws++;
        printf("[logic] تعادل %d/%d\n", s->draws, RPSP_DEADLOCK_THRESHOLD);
        if (s->draws >= RPSP_DEADLOCK_THRESHOLD) {
            printf("[logic] DEADLOCK\n");
            send_type(sockfd, s, RPSP_DEADLOCK, MOVE_NONE, src_ip);
            s->state = STATE_DEADLOCK;
        } else {
            s->state = STATE_WAITING_MOVE;
        }
        return;
    }
 
    send_type(sockfd, s, RPSP_RESULT, MOVE_NONE, src_ip);
    s->state = (result == RESULT_WIN) ? STATE_SENDING_DATA
                                      : STATE_RECEIVING_DATA;
    printf("[logic] %s\n", result == RESULT_WIN ? "فزت" : "خسرت");
}
 
static void on_result(rpsp_session_t *s, uint8_t result_byte) {
    if (s->state != STATE_WAITING_RESULT) return;
    rpsp_result_t result = (rpsp_result_t)result_byte;
    if (result == RESULT_WIN) {
        s->state = STATE_SENDING_DATA;
    } else if (result == RESULT_LOSE) {
        s->state = STATE_RECEIVING_DATA;
    } else {
        s->draws++;
        s->state = (s->draws >= RPSP_DEADLOCK_THRESHOLD)
                   ? STATE_DEADLOCK : STATE_WAITING_MOVE;
    }
}
 
static void on_deadlock(rpsp_session_t *s) {
    printf("[logic] DEADLOCK\n");
    s->state = STATE_DONE;
}
 
static void on_fin(rpsp_session_t *s, int sockfd, const char *src_ip) {
    printf("[logic] FIN\n");
    send_type(sockfd, s, RPSP_FIN, MOVE_NONE, src_ip);
    s->state = STATE_DONE;
}
 
/* ─────────────────────────────────────────────
   Main Handler
───────────────────────────────────────────── */
 
void rpsp_handle_packet(rpsp_session_t *s, int sockfd,
                        uint8_t *packet, size_t len,
                        const char *src_ip) {
    if (len < (size_t)RPSP_HEADER_SIZE) return;
    if (packet[0] != RPSP_MAGIC)        return;
 
    rpsp_type_t type        = (rpsp_type_t)packet[1];
    rpsp_move_t their_move  = (rpsp_move_t)RPSP_GET_MOVE(packet[2]);
    uint8_t     result_byte = (len > (size_t)RPSP_HEADER_SIZE)
                              ? packet[RPSP_HEADER_SIZE] : 0;
 
    switch (type) {
        case RPSP_HELLO:    on_hello(s, sockfd, src_ip);            break;
        case RPSP_ACCEPT:   on_accept(s, sockfd, src_ip);           break;
        case RPSP_MOVE:     on_move(s, sockfd, their_move, src_ip); break;
        case RPSP_RESULT:   on_result(s, result_byte);              break;
        case RPSP_DEADLOCK: on_deadlock(s);                         break;
        case RPSP_FIN:      on_fin(s, sockfd, src_ip);              break;
        default:
            printf("[logic] unknown: 0x%02X\n", (uint8_t)type);
            break;
    }
}
 
/* ─────────────────────────────────────────────
   Client Starter
───────────────────────────────────────────── */
 
void rpsp_client_start(rpsp_session_t *s, int sockfd, const char *dest_ip) {
    printf("[logic] HELLO → %s\n", dest_ip);
    s->state = STATE_HANDSHAKE;
    send_type(sockfd, s, RPSP_HELLO, MOVE_NONE, dest_ip);
}
 
