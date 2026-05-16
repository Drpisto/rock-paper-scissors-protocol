
#include "rps.h"
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <socket.c>
/* ─── Session ────────────────────────────────────────────── */
 
typedef enum {
    ROLE_SERVER,
    ROLE_CLIENT,
} rpsp_role_t;


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
 
typedef struct {
    rpsp_state_t state;
    rpsp_role_t  role;
    uint16_t     session_id;
    rpsp_move_t  my_move;
    rpsp_move_t  their_move;
    uint8_t      round;
    uint8_t      draws;
} rpsp_session_t;
 
/* ─── Logic ────────────────────────────────────────────── */


rpsp_result_t determine_result(rpsp_move_t my_move, rpsp_move_t their_move) {
    if (my_move == their_move) return RESULT_DRAW;
    if ((my_move == MOVE_ROCK     && their_move == MOVE_SCISSORS) ||
        (my_move == MOVE_PAPER    && their_move == MOVE_ROCK)     ||
        (my_move == MOVE_SCISSORS && their_move == MOVE_PAPER)) {
        return RESULT_WIN;
    }
    return RESULT_LOSE;
}

rpsp_move_t parse_move(uint8_t move_bits) {
    switch (move_bits) {
        case 0x00: return MOVE_NONE;
        case 0x01: return MOVE_ROCK;
        case 0x02: return MOVE_PAPER;
        case 0x03: return MOVE_SCISSORS;
        default:   return MOVE_NONE; // Invalid move
    }
}

void server_handle_move(rpsp_session_t *session) {
    rpsp_result_t result = determine_result(session->my_move, session->their_move);
    if (result == RESULT_DRAW) {
        session->draws++;
        if (session->draws >= RPSP_DEADLOCK_THRESHOLD) {
            session->state = STATE_DEADLOCK;
        } else {
            session->state = STATE_WAITING_MOVE; // Remain in move phase
        }
    } else {
        session->state = STATE_WAITING_RESULT;
    }
}

void client_handle_result(rpsp_session_t *session, rpsp_result_t result) {
    if (result == RESULT_DRAW) {
        session->draws++;
        if (session->draws >= RPSP_DEADLOCK_THRESHOLD) {
            session->state = STATE_DEADLOCK;
        } else {
            session->state = STATE_WAITING_MOVE; // Remain in move phase
        }
    } else {
        session->state = STATE_WAITING_RESULT;
    }
}





