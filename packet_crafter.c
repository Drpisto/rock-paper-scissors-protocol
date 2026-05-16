#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include "rps.h"



uint16_t calculate_checksum_ip(const ip_header *ip);
uint16_t calculate_checksum_rps(const rps_header *rps, const uint8_t payload[], size_t payload_len);

uint8_t *assembl_packet(rps_header *rps, ip_header *ip, const uint8_t *payload, size_t payload_len) {
    size_t size = sizeof(*rps) + sizeof(*ip) + payload_len;
    uint8_t *packet = malloc(size);
    if (!packet) return NULL;
    ip->header_checksum = calculate_checksum_ip(ip);
    rps->checksum = calculate_checksum_rps(rps, payload, payload_len);

    memcpy(packet, ip, sizeof(*ip));
    memcpy(packet + sizeof(*ip), rps, sizeof(*rps));
    memcpy(packet + sizeof(*ip) + sizeof(*rps), payload, payload_len);
    return packet; 
}

rps_header create_rps_header(uint8_t type, uint8_t move_round,
                             uint16_t session_id, uint16_t checksum) {
    rps_header header;
    header.magic = 0x52;
    header.type = type;
    header.move_round = move_round;
    header.session_id = session_id;
    header.checksum = checksum;
    return header;
}

ip_header create_ip_header( uint8_t header_length,
                           uint8_t service_type, uint16_t total_length,
                           uint16_t identification, uint16_t flags_and_offset,
                            uint8_t protocol,
                           uint16_t header_checksum, uint32_t source_ip,
                           uint32_t destination_ip) {
    ip_header header;
    header.version = 4;
    header.header_length = header_length;
    header.service_type = service_type;
    header.total_length = htons(total_length);
    header.identification = htons(identification);
    header.flags_and_offset = htons(flags_and_offset);
    header.time_to_live = 32;
    header.protocol = protocol;
    header.header_checksum = header_checksum;
    header.source_ip = htonl(source_ip);
    header.destination_ip = htonl(destination_ip);
    return header;
}

uint16_t calculate_checksum_rps(const rps_header *rps,
                            const uint8_t payload[],
                            size_t payload_len) {
    uint32_t sum = 0;
    const uint8_t *data = (const uint8_t *)rps;

    for (size_t i = 0; i < sizeof(*rps); i++) sum += data[i];
    for (size_t i = 0; i < payload_len; i++) sum += payload[i];

    while (sum >> 16) sum = (sum & 0xFFFFu) + (sum >> 16);
    return (uint16_t)(~sum);
}


uint16_t calculate_checksum_ip(const ip_header *ip) {
    ip_header temp = *ip;
    temp.header_checksum = 0;
    uint8_t buf[sizeof(ip_header)];
    memcpy(buf, &temp, sizeof(ip_header));
    const uint16_t *ptr = (const uint16_t *)buf;
    uint32_t sum = 0;
    for (size_t i = 0; i < sizeof(ip_header) / 2; i++) sum += ptr[i];
    while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
    return (uint16_t)(~sum);
}

