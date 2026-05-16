#include "rps.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <socket.c>
#include <packet_crafter.c>
#include <logic.c>

void rpsp_handel(int sockfd, uint8_t *buffer, size_t buffer_len,
                  char *src_ip, size_t src_ip_len)
{
    ssize_t result = rpsp_recv(sockfd, buffer, buffer_len, src_ip, src_ip_len);
    

    ip_header *ip = (ip_header *)buffer;
    rps_header *rpsp = (rps_header *)(buffer + (ip->header_length * 4));
    ip_header ip_header_r = create_ip_header(5, 0, sizeof(rps_header), rand() % 65536, 0, RPSP_PROTO, 0, inet_addr(src_ip), inet_addr("127.0.0.1"));
    int socket_o  = rpsp_socket_open();
    // Handle HELLO
    if (rpsp->type == 0x01) {
        printf("Received HELLO from %s\n", src_ip);
        char send_accpent;
        printf("Accept? (y/n): ");
        scanf(" %c", &send_accpent);
        if (send_accpent == 'y' || send_accpent == 'Y') {
            
            rps_header rps_header_r= create_rps_header(RPSP_ACCEPT, 0, rpsp->session_id, 0);
            uint8_t *respond_packet = assembl_packet(&rps_header_r, &ip_header_r, NULL, 0);
            rpsp_send(socket_o, respond_packet, sizeof(ip_header) + sizeof(rps_header), src_ip);
            free(respond_packet);
        } else {
            rps_header rps_header_r= create_rps_header(RPSP_REJECT, 0, rpsp->session_id, 0);

            uint8_t *respond_packet = assembl_packet(&rps_header_r, &ip_header_r, NULL, 0);
            
            rpsp_send(socket_o, respond_packet, sizeof(ip_header) + sizeof(rps_header), src_ip);
            free(respond_packet);

            printf("Rejected HELLO from %s\n", src_ip);
        }


    }
    // Handle MOVE
    if (rpsp->type == 0x04) {
        printf("Received MOVE from %s\n", src_ip);
        rpsp_move_t my_move = get_move_user();
        rpsp_move_t opponent_move = (rpsp->move_round >> 6) & 0x03;
        rpsp_result_t game_result = det_result(my_move, opponent_move);
        rps_header rps_header_r= create_rps_header(RPSP_RESULT, (my_move << 6) | (rpsp->move_round & 0x3F), rpsp->session_id, 0);
        uint8_t *respond_packet = assembl_packet(&rps_header_r, &ip_header_r, NULL, 0);
        rpsp_send(socket_o, respond_packet, sizeof(ip_header) + sizeof(rps_header), src_ip);
        free(respond_packet);
        printf("You %s!\n", game_result == RESULT_WIN ? "win" : game_result == RESULT_LOSE ? "lose" : "draw");

    }
    // Handle TAUNT
    if (rpsp->type == 0x08) {
        printf("the opponent taunted you: he say %.*s\n", result, rpsp->payload);
    }
    // Handle data
    if (rpsp->type == 0x08) {
        printf("the opponent send you data: %.*s\n", result, rpsp->payload);
        rps_header rps_header_r= create_rps_header(RPSP_ACK, 0, rpsp->session_id, 0);
        uint8_t *respond_packet = assembl_packet(&rps_header_r, &ip_header_r, NULL, 0);
        rpsp_send(socket_o, respond_packet, sizeof(ip_header) + sizeof(rps_header), src_ip);
        free(respond_packet);
    }
    // Handle FIN
    if (rpsp->type == 0x0A) {
        printf("the opponent ended the game.\n");
        rps_header rps_header_r= create_rps_header(RPSP_FIN, 0, rpsp->session_id, 0);
        uint8_t *respond_packet = assembl_packet(&rps_header_r, &ip_header_r, NULL, 0);
        rpsp_send(socket_o, respond_packet, sizeof(ip_header) + sizeof(rps_header), src_ip);
        free(respond_packet);}

    // Handle RST
    if (rpsp->type == 0xFF) {
        printf("the opponent finish the game.\n");
        rps_header rps_header_r= create_rps_header(RPSP_RST, 0, rpsp->session_id, 0);
        uint8_t *respond_packet = assembl_packet(&rps_header_r, &ip_header_r, NULL, 0);
        rpsp_send(socket_o, respond_packet, sizeof(ip_header) + sizeof(rps_header), src_ip);
        free(respond_packet);
        
    }

}




                 

