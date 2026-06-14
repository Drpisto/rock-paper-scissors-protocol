#include "rps.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

// Forward declarations
uint16_t calculate_checksum_rps(const rps_header *rps, const uint8_t payload[], size_t payload_len);
ssize_t rpsp_recv(int sockfd, uint8_t *buffer, size_t buffer_len, char *src_ip, size_t src_ip_len);

int rpsp_socket_open(void) {
    int sockfd = socket(AF_INET, SOCK_RAW, RPSP_PROTO);
    if (sockfd < 0) {
        perror("socket");
        return -1;
    }
    return sockfd;
}

int rpsp_send(int sockfd, const uint8_t *packet, size_t packet_len,
              const char *dest_ip) {
    struct sockaddr_in dest_addr;
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port   = 0;
    inet_pton(AF_INET, dest_ip, &dest_addr.sin_addr);

    ssize_t sent = sendto(sockfd, packet, packet_len, 0,
                          (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    if (sent < 0) {
        perror("sendto");
        return -1;
    }

    return (int)sent;
}

void rpsp_socket_flush(int sockfd) {
    uint8_t discard_buf[4096];
    // Read all pending packets in non-blocking mode until none are left
    while (recv(sockfd, discard_buf, sizeof(discard_buf), MSG_DONTWAIT) > 0) {
        // Discarding old/timed out packets
    }
}

int rpsp_wait_packet(int sockfd, uint16_t session_id, rpsp_type_t expected_type, rps_header *out_hdr, uint8_t *out_payload, size_t *out_payload_len, int timeout_sec) {
    struct timeval tv = {timeout_sec, 0};
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    
    uint8_t buffer[65536];
    char src_ip[INET_ADDRSTRLEN];
    
    while (1) {
        ssize_t received = rpsp_recv(sockfd, buffer, sizeof(buffer), src_ip, sizeof(src_ip));
        if (received < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return -1; // Timeout
            }
            return -2; // Error
        }
        if (received == 0) {
            continue; // Discarded by rpsp_recv due to checksum or magic mismatch
        }
        
        struct iphdr *ip = (struct iphdr *)buffer;
        int ip_hdr_len = ip->ihl * 4;
        rps_header *rpsp = (rps_header *)(buffer + ip_hdr_len);
        
        if (rpsp->session_id != session_id) {
            continue; // Ignore other sessions
        }
        
        if (rpsp->type == RPSP_REJECT || rpsp->type == RPSP_RST || rpsp->type == RPSP_DEADLOCK || rpsp->type == RPSP_FIN) {
            if (out_hdr) *out_hdr = *rpsp;
            return rpsp->type;
        }
        
        if (rpsp->type == expected_type) {
            if (out_hdr) *out_hdr = *rpsp;
            size_t pay_len = received - ip_hdr_len - sizeof(rps_header);
            if (out_payload && pay_len > 0) {
                memcpy(out_payload, rpsp->payload, pay_len);
            }
            if (out_payload_len) *out_payload_len = pay_len;
            return 0; // Success
        }
    }
}

ssize_t rpsp_recv(int sockfd, uint8_t *buffer, size_t buffer_len,
                  char *src_ip, size_t src_ip_len) {
    ssize_t received = recv(sockfd, buffer, buffer_len, 0);
    if (received < 0) {
        perror("recv");
        return -1;
    }

    struct iphdr *ip = (struct iphdr *)buffer;
    int ip_hdr_len = ip->ihl * 4;
    
    // Ensure we received at least the IP header and the RPS header
    if (received < ip_hdr_len + (ssize_t)sizeof(rps_header)) {
        return 0; // Packet too short, discard
    }

    uint8_t *rpsp_ptr = buffer + ip_hdr_len;
    rps_header *rpsp = (rps_header *)rpsp_ptr;

    if (rpsp->magic != RPSP_MAGIC) {
        return 0; // Not our packet, discard
    }

    // Verify Checksum
    size_t payload_len = received - ip_hdr_len - sizeof(rps_header);
    uint16_t received_checksum = rpsp->checksum;
    
    rps_header temp_hdr = *rpsp;
    temp_hdr.checksum = 0;
    uint16_t calc_checksum = calculate_checksum_rps(&temp_hdr, rpsp->payload, payload_len);
    
    if (received_checksum != calc_checksum) {
        // Print warning but we can drop the packet or accept it. Let's drop it to enforce correctness.
        printf("[RPSP] Checksum mismatch! Expected 0x%04X, got 0x%04X. Dropping packet.\n", calc_checksum, received_checksum);
        return 0;
    }

    if (src_ip && src_ip_len > 0) {
        inet_ntop(AF_INET, &ip->saddr, src_ip, (socklen_t)src_ip_len);
    }

    return received;
}

void rpsp_socket_close(int sockfd) {
    close(sockfd);
}