#include "rps.h"
#include "socket.c"
#include "packet_crafter.c"
#include "logic.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <arpa/inet.h>

int rpsp_server(void)
{
    int sockfd = rpsp_socket_open();
    if (sockfd < 0) return -1;

    printf("Server started. Waiting for connections...\n");

    uint8_t buffer[65536];
    char src_ip[INET_ADDRSTRLEN];

    while (1) {
        ssize_t result = rpsp_recv(sockfd, buffer, sizeof(buffer), src_ip, sizeof(src_ip));
        if (result <= 0) continue;

        ip_header *ip = (ip_header *)buffer;
        rps_header *rpsp = (rps_header *)(buffer + (ip->header_length * 4));
        ip_header ip_header_r = create_ip_header(5, 0, sizeof(rps_header), rand() % 65536, 0, RPSP_PROTO, 0, inet_addr(src_ip), inet_addr("127.0.0.1"));

        if (rpsp->type == RPSP_HELLO) {
            printf("Received HELLO from %s\n", src_ip);
            char accept;
            printf("Accept? (y/n): ");
            scanf(" %c", &accept);
            if (accept == 'y' || accept == 'Y') {
                rps_header rps_header_r = create_rps_header(RPSP_ACCEPT, 0, rpsp->session_id, 0);
                uint8_t *respond_packet = assembl_packet(&rps_header_r, &ip_header_r, NULL, 0);
                rpsp_send(sockfd, respond_packet, sizeof(ip_header) + sizeof(rps_header), src_ip);
                free(respond_packet);
            } else {
                rps_header rps_header_r = create_rps_header(RPSP_REJECT, 0, rpsp->session_id, 0);
                uint8_t *respond_packet = assembl_packet(&rps_header_r, &ip_header_r, NULL, 0);
                rpsp_send(sockfd, respond_packet, sizeof(ip_header) + sizeof(rps_header), src_ip);
                free(respond_packet);
            }
        }
        if (rpsp->type == RPSP_MOVE) {
            printf("Received MOVE from %s\n", src_ip);
            rpsp_move_t my_move = get_move_user();
            rpsp_move_t opponent_move = (rpsp->move_round >> 6) & 0x03;
            rpsp_result_t game_result = det_result(my_move, opponent_move);
            rps_header rps_header_r = create_rps_header(RPSP_RESULT, (my_move << 6) | (rpsp->move_round & 0x3F), rpsp->session_id, 0);
            uint8_t *respond_packet = assembl_packet(&rps_header_r, &ip_header_r, NULL, 0);
            rpsp_send(sockfd, respond_packet, sizeof(ip_header) + sizeof(rps_header), src_ip);
            free(respond_packet);
            printf("You %s!\n", game_result == RESULT_WIN ? "win" : game_result == RESULT_LOSE ? "lose" : "draw");
        }
        if (rpsp->type == RPSP_TAUNT) {
            printf("Opponent taunted: %.*s\n", (int)(result - sizeof(ip_header) - sizeof(rps_header)), rpsp->payload);
        }
        if (rpsp->type == RPSP_DATA) {
            printf("Opponent sent data: %.*s\n", (int)(result - sizeof(ip_header) - sizeof(rps_header)), rpsp->payload);
            rps_header rps_header_r = create_rps_header(RPSP_ACK, 0, rpsp->session_id, 0);
            uint8_t *respond_packet = assembl_packet(&rps_header_r, &ip_header_r, NULL, 0);
            rpsp_send(sockfd, respond_packet, sizeof(ip_header) + sizeof(rps_header), src_ip);
            free(respond_packet);
        }
    }

    close(sockfd);
    return 0;
}

int main(int argc, char **argv) {
    return rpsp_server();
}




                 

