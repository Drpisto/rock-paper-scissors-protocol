#ifndef LOGIC_H
#define LOGIC_H

#include "rps.h"
#include <sys/types.h>

/* ─── Role ───────────────────────────────────── */
typedef enum {
    ROLE_SERVER,
    ROLE_CLIENT,
} rpsp_role_t;

/* ─── State ──────────────────────────────────── */
typedef enum {
    STATE_IDLE,
    STATE_HANDSHAKE,
    STATE_WAITING_MOVE,
    STATE_WAITING_RESULT,
    STATE_SENDING_DATA,
    STATE_RECEIVING_DATA,
    STATE_DEADLOCK,
    STATE_DONE,
} rpsp_state_t;

/* ─── Session ────────────────────────────────── */
typedef struct {
    rpsp_state_t state;
    rpsp_role_t  role;
    uint16_t     session_id;
    rpsp_move_t  my_move;
    rpsp_move_t  their_move;
    uint8_t      round;
    uint8_t      draws;
} rpsp_session_t;

/* ─── API ────────────────────────────────────── */
void rpsp_session_init  (rpsp_session_t *s, rpsp_role_t role);
void rpsp_handle_packet (rpsp_session_t *s, int sockfd,
                         uint8_t *packet, size_t len,
                         const char *src_ip);
void rpsp_client_start  (rpsp_session_t *s, int sockfd,
                         const char *dest_ip);

#endif /* LOGIC_H */