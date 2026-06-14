#include "rps.h"
#include "socket.c"
#include "packet_crafter.c"
#include "logic.c"
#include <stdio.h>
#include <netinet/ip.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <arpa/inet.h>

void send_hello(int sockfd, int session_id, const char *dest_ip) {
    rps_header rps_header_r = create_rps_header(RPSP_HELLO, 0, session_id, 0);
    rps_header_r.checksum = calculate_checksum_rps(&rps_header_r, NULL, 0);
    rpsp_send(sockfd, (uint8_t*)&rps_header_r, sizeof(rps_header), dest_ip);
}


int rpsp_client(const char *dest_ip_str) {
    int sockfd = rpsp_socket_open();
    if (sockfd < 0) return -1;

    int session_id = rand() % 65536;
    printf("[Client] Opened socket. Session ID: %d\n", session_id);
    
    // --- Step 1: Handshake ---
    printf("[Client] Sending HELLO...\n");
    rpsp_socket_flush(sockfd);
    send_hello(sockfd, session_id, dest_ip_str);
    
    rps_header resp_hdr;
    int status = rpsp_wait_packet(sockfd, session_id, RPSP_ACCEPT, &resp_hdr, NULL, NULL, 15);
    if (status == RPSP_REJECT) {
        printf("[Client] Connection rejected by server.\n");
        close(sockfd);
        return -1;
    } else if (status < 0) {
        printf("[Client] Handshake timed out or failed (status: %d).\n", status);
        close(sockfd);
        return -1;
    }
    printf("[Client] Connection accepted!\n");
    
    // --- Step 2: Play Rock Paper Scissors ---
    int round = 1;
    int won_game = 0;
    
    while (round <= RPSP_DEADLOCK_THRESHOLD) {
        printf("\n--- Round %d ---\n", round);
        rpsp_move_t my_move = get_move_user();
        
        uint8_t move_round = RPSP_SET_MOVE_ROUND(my_move, round);
        rps_header r_send = create_rps_header(RPSP_MOVE, move_round, session_id, 0);
        r_send.checksum = calculate_checksum_rps(&r_send, NULL, 0);
        
        rpsp_socket_flush(sockfd);
        rpsp_send(sockfd, (uint8_t*)&r_send, sizeof(rps_header), dest_ip_str);
        
        printf("[Client] Sent move. Waiting for server move and result...\n");
        rps_header res_hdr;
        status = rpsp_wait_packet(sockfd, session_id, RPSP_RESULT, &res_hdr, NULL, NULL, 20);
        if (status == RPSP_DEADLOCK) {
            printf("[Client] Connection deadlocked by server.\n");
            close(sockfd);
            return -1;
        } else if (status < 0) {
            printf("[Client] Timed out waiting for round result.\n");
            close(sockfd);
            return -1;
        }
        
        rpsp_move_t server_move = RPSP_GET_MOVE(res_hdr.move_round);
        printf("[Client] Server chose: %s\n", 
               server_move == MOVE_ROCK ? "Rock" : 
               server_move == MOVE_PAPER ? "Paper" : 
               server_move == MOVE_SCISSORS ? "Scissors" : "None");
        
        rpsp_result_t res = det_result(my_move, server_move);
        if (res == RESULT_WIN) {
            printf("[Client] You WIN! You have the right to send data.\n");
            won_game = 1;
            break;
        } else if (res == RESULT_LOSE) {
            printf("[Client] You LOSE! You must wait for the server to send data.\n");
            won_game = 0;
            break;
        } else {
            printf("[Client] Draw! Playing next round.\n");
            round++;
        }
    }
    
    if (round > RPSP_DEADLOCK_THRESHOLD) {
        printf("[Client] Deadlock reached. Connection closed.\n");
        close(sockfd);
        return -1;
    }
    
    // --- Step 3: Send or Receive Data ---
    if (won_game) {
        printf("\n--- Choose transmission type ---\n");
        printf("1. Send Data\n");
        printf("2. Send Taunt\n");
        int choice = 1;
        printf("Choice: ");
        if (scanf("%d", &choice) != 1) choice = 1;
        
        char msg[256];
        printf("Enter message: ");
        scanf(" %255[^\n]", msg);
        
        rpsp_type_t type = (choice == 2) ? RPSP_TAUNT : RPSP_DATA;
        size_t len = strlen(msg);
        size_t total = sizeof(rps_header) + len;
        uint8_t *pkt = malloc(total);
        rps_header *h = (rps_header*)pkt;
        *h = create_rps_header(type, 0, session_id, 0);
        memcpy(pkt + sizeof(rps_header), msg, len);
        h->checksum = calculate_checksum_rps(h, (uint8_t*)msg, len);
        
        rpsp_socket_flush(sockfd);
        rpsp_send(sockfd, pkt, total, dest_ip_str);
        free(pkt);
        
        printf("[Client] Message sent. Waiting for ACK...\n");
        status = rpsp_wait_packet(sockfd, session_id, RPSP_ACK, NULL, NULL, NULL, 10);
        if (status == 0) {
            printf("[Client] Message acknowledged by server successfully!\n");
        } else {
            printf("[Client] Failed to receive ACK (status: %d).\n", status);
        }
    } else {
        printf("\n[Client] Waiting for server data...\n");
        rps_header data_hdr;
        uint8_t payload[4096];
        size_t payload_len = 0;
        status = rpsp_wait_packet(sockfd, session_id, RPSP_DATA, &data_hdr, payload, &payload_len, 30);
        if (status == 0) {
            printf("[Client] Received DATA from server: %.*s\n", (int)payload_len, payload);
            
            // Send ACK
            rps_header ack_hdr = create_rps_header(RPSP_ACK, 0, session_id, 0);
            ack_hdr.checksum = calculate_checksum_rps(&ack_hdr, NULL, 0);
            rpsp_send(sockfd, (uint8_t*)&ack_hdr, sizeof(ack_hdr), dest_ip_str);
            printf("[Client] Sent ACK to server.\n");
        } else if (rpsp_wait_packet(sockfd, session_id, RPSP_TAUNT, &data_hdr, payload, &payload_len, 1) == 0) {
            printf("[Client] Received TAUNT from server: %.*s\n", (int)payload_len, payload);
            
            // Send ACK
            rps_header ack_hdr = create_rps_header(RPSP_ACK, 0, session_id, 0);
            ack_hdr.checksum = calculate_checksum_rps(&ack_hdr, NULL, 0);
            rpsp_send(sockfd, (uint8_t*)&ack_hdr, sizeof(ack_hdr), dest_ip_str);
            printf("[Client] Sent ACK to server.\n");
        } else {
            printf("[Client] Timed out or failed waiting for server data.\n");
        }
    }
    
    // --- Step 4: Finish connection ---
    rps_header fin_hdr = create_rps_header(RPSP_FIN, 0, session_id, 0);
    fin_hdr.checksum = calculate_checksum_rps(&fin_hdr, NULL, 0);
    rpsp_send(sockfd, (uint8_t*)&fin_hdr, sizeof(fin_hdr), dest_ip_str);
    
    close(sockfd);
    printf("[Client] Connection closed.\n");
    return 0;
}

int main(int argc, char **argv) {
    srand((unsigned)time(NULL));
    if (argc < 2) {
        fprintf(stderr, "usage: %s <server_ip>\n", argv[0]);
        return 1;
    }
    return rpsp_client(argv[1]);
}

