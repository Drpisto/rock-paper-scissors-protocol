#ifndef RPSP_H
#define RPSP_H

#include <stdint.h>
#include <stddef.h>

// ─── Constants ───────────────────────────────
#define RPSP_MAGIC    0x52
#define RPSP_VERSION  0x01
#define RPSP_PROTO    253        // IP protocol number
#define RPSP_MAX_PAYLOAD 65528
#define RPSP_HEADER_SIZE 7
#define RPSP_DEADLOCK_THRESHOLD 5

// ─── Types ───────────────────────────────────
typedef enum __attribute__((packed)) {
    RPSP_HELLO    = 0x01,
    RPSP_ACCEPT   = 0x02,
    RPSP_REJECT   = 0x03,
    RPSP_MOVE     = 0x04,
    RPSP_RESULT   = 0x05,
    RPSP_DATA     = 0x06,
    RPSP_ACK      = 0x07,
    RPSP_TAUNT    = 0x08,
    RPSP_DEADLOCK = 0x09,
    RPSP_FIN      = 0x0A,
    RPSP_RST      = 0xFF,
} rpsp_type_t;

typedef enum __attribute__((packed)) {
    MOVE_NONE     = 0x00,
    MOVE_ROCK     = 0x01,
    MOVE_PAPER    = 0x02,
    MOVE_SCISSORS = 0x03,
} rpsp_move_t;

typedef enum __attribute__((packed)) {
    RESULT_WIN  = 0x01,
    RESULT_LOSE = 0x02,
    RESULT_DRAW = 0x03,
} rpsp_result_t;

// ─── Structs ──────────────────────────────────
typedef struct __attribute__((packed)) {
    uint8_t  magic;
    uint8_t  type;
    uint8_t  move_round;    // bit7-6=move, bit5-0=round
    uint16_t session_id;
    uint16_t checksum;
    uint8_t  payload[];
} rps_header;
typedef struct __attribute__((packed)) {
    uint8_t  version;
    uint8_t  header_length;
    uint8_t  service_type;
    uint16_t total_length;
    uint16_t identification;
    uint16_t flags_and_offset;
    uint8_t  time_to_live;
    uint8_t  protocol;
    uint16_t header_checksum;
    uint32_t source_ip;
    uint32_t destination_ip;
} ip_header;

// ─── Macros ───────────────────────────────────
#define RPSP_GET_MOVE(mr)     ((mr) >> 6)
#define RPSP_GET_ROUND(mr)    ((mr) & 0x3F)
#define RPSP_SET_MOVE_ROUND(m, r) (((m) << 6) | ((r) & 0x3F))

#endif // RPSP_H