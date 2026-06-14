#include "rps.h"
#include "socket.c"
#include "packet_crafter.c"
#include "logic.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <arpa/inet.h>
#include <errno.h>
#include <unistd.h>

void handle_session(int sockfd, uint16_t session_id, const char *client_ip) {
    printf("[Server] Handshake accepted. Starting game with %s (Session ID: %d)\n", client_ip, session_id);
    
    int round = 1;
    int won_game = 0; // 1 = client won, 2 = server won
    
    while (round <= RPSP_DEADLOCK_THRESHOLD) {
        printf("[Server] Waiting for client move in round %d...\n", round);
        
        rps_header move_hdr;
        int status = rpsp_wait_packet(sockfd, session_id, RPSP_MOVE, &move_hdr, NULL, NULL, 30);
        if (status < 0) {
            printf("[Server] Timed out or failed waiting for client move. Terminating session.\n");
            return;
        }
        
        rpsp_move_t client_move = RPSP_GET_MOVE(move_hdr.move_round);
        uint8_t client_round = RPSP_GET_ROUND(move_hdr.move_round);
        printf("[Server] Client chose: %s (Round: %d)\n", 
               client_move == MOVE_ROCK ? "Rock" : 
               client_move == MOVE_PAPER ? "Paper" : 
               client_move == MOVE_SCISSORS ? "Scissors" : "None", 
               client_round);
        
        rpsp_move_t my_move = (rpsp_move_t)((rand() % 3) + 1);
        printf("[Server] Server chose: %s\n", 
               my_move == MOVE_ROCK ? "Rock" : 
               my_move == MOVE_PAPER ? "Paper" : 
               my_move == MOVE_SCISSORS ? "Scissors" : "None");
               
        rpsp_result_t game_result = det_result(my_move, client_move); // Server perspective
        
        // Send RESULT packet
        uint8_t move_round = (my_move << 6) | (client_round & 0x3F);
        rps_header r_res = create_rps_header(RPSP_RESULT, move_round, session_id, 0);
        r_res.checksum = calculate_checksum_rps(&r_res, NULL, 0);
        rpsp_send(sockfd, (uint8_t*)&r_res, sizeof(r_res), client_ip);
        
        if (game_result == RESULT_WIN) {
            printf("[Server] Server won this round! Server will send data.\n");
            won_game = 2; // Server won
            break;
        } else if (game_result == RESULT_LOSE) {
            printf("[Server] Server lost this round! Client will send data.\n");
            won_game = 1; // Client won
            break;
        } else {
            printf("[Server] Draw! Advancing round.\n");
            round++;
        }
    }
    
    if (round > RPSP_DEADLOCK_THRESHOLD) {
        printf("[Server] Deadlock reached. Terminating session.\n");
        rps_header deadlock_hdr = create_rps_header(RPSP_DEADLOCK, 0, session_id, 0);
        deadlock_hdr.checksum = calculate_checksum_rps(&deadlock_hdr, NULL, 0);
        rpsp_send(sockfd, (uint8_t*)&deadlock_hdr, sizeof(deadlock_hdr), client_ip);
        return;
    }
    
    // --- Step 3: Send or Receive Data ---
    if (won_game == 1) { // Client won, server receives
        printf("[Server] Waiting for client data...\n");
        rps_header data_hdr;
        uint8_t payload[4096];
        size_t payload_len = 0;
        
        // Wait for either DATA or TAUNT
        int status = rpsp_wait_packet(sockfd, session_id, RPSP_DATA, &data_hdr, payload, &payload_len, 30);
        if (status != 0) {
            // Check if it's a taunt
            status = rpsp_wait_packet(sockfd, session_id, RPSP_TAUNT, &data_hdr, payload, &payload_len, 1);
        }
        
        if (status == 0) {
            printf("[Server] Received %s: %.*s\n", 
                   data_hdr.type == RPSP_TAUNT ? "TAUNT" : "DATA", 
                   (int)payload_len, payload);
            
            // Send ACK
            rps_header ack_hdr = create_rps_header(RPSP_ACK, 0, session_id, 0);
            ack_hdr.checksum = calculate_checksum_rps(&ack_hdr, NULL, 0);
            rpsp_send(sockfd, (uint8_t*)&ack_hdr, sizeof(ack_hdr), client_ip);
            printf("[Server] Sent ACK.\n");
        } else {
            printf("[Server] Failed to receive data from client.\n");
        }
    } else if (won_game == 2) { // Server won, server sends
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
        rpsp_send(sockfd, pkt, total, client_ip);
        free(pkt);
        
        printf("[Server] Sent message. Waiting for ACK...\n");
        int status = rpsp_wait_packet(sockfd, session_id, RPSP_ACK, NULL, NULL, NULL, 15);
        if (status == 0) {
            printf("[Server] Message acknowledged by client successfully!\n");
        } else {
            printf("[Server] Failed to receive ACK (status: %d).\n", status);
        }
    }
    
    // --- Step 4: Wait for FIN ---
    printf("[Server] Waiting for client FIN...\n");
    rpsp_wait_packet(sockfd, session_id, RPSP_FIN, NULL, NULL, NULL, 10);
    printf("[Server] Session %d finished.\n\n", session_id);
}

int rpsp_server(void) {
    srand((unsigned)time(NULL));
    int sockfd = rpsp_socket_open();
    if (sockfd < 0) return -1;

    // Set a timeout on the listening socket so we can gracefully poll/exit if needed
    struct timeval tv = {1, 0};
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    printf("Server started. Waiting for connections...\n");

    uint8_t buffer[65536];
    char src_ip[INET_ADDRSTRLEN];

    while (1) {
        ssize_t result = rpsp_recv(sockfd, buffer, sizeof(buffer), src_ip, sizeof(src_ip));
        if (result <= 0) {
            // Timeout or discarded packet
            continue;
        }

        struct iphdr *ip = (struct iphdr *)buffer;
        int ip_hdr_len = ip->ihl * 4;
        rps_header *rpsp = (rps_header *)(buffer + ip_hdr_len);

        if (rpsp->type == RPSP_HELLO) {
            printf("\n[Server] Received HELLO from %s (Session: %d)\n", src_ip, rpsp->session_id);
            char accept_choice;
            printf("Accept connection? (y/n): ");
            fflush(stdout);
            if (scanf(" %c", &accept_choice) != 1) accept_choice = 'n';
            
            if (accept_choice == 'y' || accept_choice == 'Y') {
                rps_header r = create_rps_header(RPSP_ACCEPT, 0, rpsp->session_id, 0);
                r.checksum = calculate_checksum_rps(&r, NULL, 0);
                rpsp_send(sockfd, (uint8_t*)&r, sizeof(r), src_ip);
                
                // Flush before entering session to clear loopback echo of ACCEPT
                rpsp_socket_flush(sockfd);
                handle_session(sockfd, rpsp->session_id, src_ip);
            } else {
                rps_header r = create_rps_header(RPSP_REJECT, 0, rpsp->session_id, 0);
                r.checksum = calculate_checksum_rps(&r, NULL, 0);
                rpsp_send(sockfd, (uint8_t*)&r, sizeof(r), src_ip);
                printf("[Server] Rejected connection from %s.\n", src_ip);
            }
        }
    }

    close(sockfd);
    return 0;
}

int main(int argc, char **argv) {
    return rpsp_server();
}
