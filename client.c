#include "rps.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <socket.c>
#include <packet_crafter.c>
#include <logic.c>

void rpsp_client(ssize_t des_ip)
{
    //show menu
    printf("Welcome to Rock-Paper-Scissors Protocol Client!\n");

    printf("1. send hello\n");
    printf("2. send move\n");
    printf("3. send taunt\n");
    printf("4. send data\n");

    ip_header ip_header_r = create_ip_header(5, 0, sizeof(rps_header), rand() % 65536, 0, RPSP_PROTO, 0, inet_addr("127.0.0.1"), inet_addr(des_ip));
    int session_id = rand() % 65536;
    int choice;

    printf("Enter your choice: ");
    scanf("%d", &choice);
    if (choice == 1) {
        
        
        send_hello(des_ip, ip_header_r, session_id);
    } else if (choice == 2) {
        send_move(des_ip, ip_header_r, session_id);
    } else if (choice == 3) {
        // send taunt
        send_taunt(ip_header_r, session_id);
    }
    else if (choice == 4) {
        // send data
        send_data(ip_header_r, session_id);
    } else {
        printf("Invalid choice.\n");
    }
    
    // handel response
    uint8_t buffer[65536];
    char src_ip[INET_ADDRSTRLEN];
    rpsp_handel(rpsp_socket_open(), buffer, sizeof(buffer), src_ip, sizeof(src_ip));    
    ssize_t result  = rpsp_recv(rpsp_socket_open(), buffer, sizeof(buffer), src_ip, sizeof(src_ip));
    ip_header *ip = (ip_header *)buffer;
    rps_header *rpsp = (rps_header *)(buffer + (ip->header_length * 4));
    if (rpsp->type == RPSP_RESULT){
        rpsp_move_t opponent_move = (rpsp->move_round >> 6) & 0x03;
        rpsp_result_t game_result = det_result(get_move_user(), opponent_move);
        printf("You %s!\n", game_result == RESULT_WIN ? "win you can send data" : game_result == RESULT_LOSE ? "lose try again" : "draw");
    }

    if (rpsp->type == RPSP_REJECT) {
        printf("Your connection was rejected by the server.\n");
    }

    if (rpsp->type == RPSP_FIN) {
        printf("Your connection was closed by the server.\n");
    }

    if (rpsp->type == RPSP_RST) {
        printf("Your connection was reset by the server.\n");
    }
    
}

void send_hello(int sockfd , ip_header ip_header_r, int session_id) {
    rps_header rps_header_r= create_rps_header(RPSP_HELLO, 0, session_id, 0);
    uint8_t *respond_packet = assembl_packet(&rps_header_r, &ip_header_r, NULL, 0);
    rpsp_send(sockfd, respond_packet, sizeof(ip_header) + sizeof(rps_header), ip_header_r.destination_ip);
    free(respond_packet);
}

void send_move(int sockfd , ip_header ip_header_r , int session_id ) {
    // send  Move
    rpsp_move_t my_move = get_move_user();
    my_move = (my_move << 6) & 0xC0; // Move in the upper 2 bits

    rps_header rps_header_r_move= create_rps_header(RPSP_MOVE, my_move, session_id, 0);
    uint8_t *respond_packet_move = assembl_packet(&rps_header_r_move, &ip_header_r, NULL, 0);
    rpsp_send(sockfd, respond_packet_move, sizeof(ip_header) + sizeof(rps_header), ip_header_r.destination_ip);
    free(respond_packet_move);
}

void send_data(ip_header ip_header_r, int session_id) {
    char data[256];
    printf("Enter your data: ");
    scanf(" %255[^\n]", data);
    rps_header rps_header_r= create_rps_header(RPSP_DATA, 0, session_id, 0);
    uint8_t *respond_packet = assembl_packet(&rps_header_r, &ip_header_r, (uint8_t *)data, strlen(data));
    rpsp_send(rpsp_socket_open(), respond_packet, sizeof(ip_header) + sizeof(rps_header) + strlen(data), ip_header_r.destination_ip);
    free(respond_packet);
}
void send_taunt(ip_header ip_header_r, int session_id) {
    char taunt[256];
    printf("Enter your taunt: ");
    scanf(" %255[^\n]", taunt);
    rps_header rps_header_r= create_rps_header(RPSP_TAUNT, 0, session_id, 0);
    uint8_t *respond_packet = assembl_packet(&rps_header_r, &ip_header_r, (uint8_t *)taunt, strlen(taunt));
    rpsp_send(rpsp_socket_open(), respond_packet, sizeof(ip_header) + sizeof(rps_header) + strlen(taunt), ip_header_r.destination_ip);
    free(respond_packet);
}
