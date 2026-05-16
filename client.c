#include "rps.h"
#include "socket.c"
#include "packet_crafter.c"
#include "logic.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <arpa/inet.h>

void send_hello(int, ip_header, int, const char*);
void send_move(int, ip_header, int, const char*);
void send_taunt(int, ip_header, int, const char*);
void send_data(int, ip_header, int, const char*);

int rpsp_client(const char *dest_ip_str)
{
    int sockfd = rpsp_socket_open();
    if (sockfd < 0) return -1;

    printf("Welcome to Rock-Paper-Scissors Protocol Client!\n");
    printf("1. send hello\n");
    printf("2. send move\n");
    printf("3. send taunt\n");
    printf("4. send data\n");

    ip_header ip_header_r = create_ip_header(5, 0, sizeof(rps_header), rand() % 65536, 0, RPSP_PROTO, 0, inet_addr("127.0.0.1"), inet_addr(dest_ip_str));
    int session_id = rand() % 65536;
    int choice;

    printf("Enter your choice: ");
    scanf("%d", &choice);

    if (choice == 1) {
        send_hello(sockfd, ip_header_r, session_id, dest_ip_str);
    } else if (choice == 2) {
        send_move(sockfd, ip_header_r, session_id, dest_ip_str);
    } else if (choice == 3) {
        send_taunt(sockfd, ip_header_r, session_id, dest_ip_str);
    } else if (choice == 4) {
        send_data(sockfd, ip_header_r, session_id, dest_ip_str);
    } else {
        printf("Invalid choice.\n");
    }

    uint8_t buffer[65536];
    char src_ip[INET_ADDRSTRLEN];
    ssize_t result = rpsp_recv(sockfd, buffer, sizeof(buffer), src_ip, sizeof(src_ip));

    if (result > 0) {
        ip_header *ip = (ip_header *)buffer;
        rps_header *rpsp = (rps_header *)(buffer + (ip->header_length * 4));
        if (rpsp->type == RPSP_RESULT){
            rpsp_move_t opponent_move = (rpsp->move_round >> 6) & 0x03;
            rpsp_result_t game_result = det_result(get_move_user(), opponent_move);
            printf("You %s!\n", game_result == RESULT_WIN ? "win you can send data" : game_result == RESULT_LOSE ? "lose try again" : "draw");
        }
        if (rpsp->type == RPSP_REJECT) printf("Your connection was rejected by the server.\n");
        if (rpsp->type == RPSP_FIN) printf("Your connection was closed by the server.\n");
        if (rpsp->type == RPSP_RST) printf("Your connection was reset by the server.\n");
    }

    close(sockfd);
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: %s <server_ip>\n", argv[0]);
        return 1;
    }
    return rpsp_client(argv[1]);
}

void send_hello(int sockfd, ip_header ip_header_r, int session_id, const char *dest_ip) {
    rps_header rps_header_r = create_rps_header(RPSP_HELLO, 0, session_id, 0);
    uint8_t *respond_packet = assembl_packet(&rps_header_r, &ip_header_r, NULL, 0);
    rpsp_send(sockfd, respond_packet, sizeof(ip_header) + sizeof(rps_header), dest_ip);
    free(respond_packet);
}

void send_move(int sockfd , ip_header ip_header_r , int session_id, const char *dest_ip) {
    rpsp_move_t my_move = get_move_user();
    my_move = (my_move << 6) & 0xC0;

    rps_header rps_header_r_move= create_rps_header(RPSP_MOVE, my_move, session_id, 0);
    uint8_t *respond_packet_move = assembl_packet(&rps_header_r_move, &ip_header_r, NULL, 0);
    rpsp_send(sockfd, respond_packet_move, sizeof(ip_header) + sizeof(rps_header), dest_ip);
    free(respond_packet_move);
}

void send_data(int sockfd, ip_header ip_header_r, int session_id, const char *dest_ip) {
    char data[256];
    printf("Enter your data: ");
    scanf(" %255[^\n]", data);
    rps_header rps_header_r= create_rps_header(RPSP_DATA, 0, session_id, 0);
    uint8_t *respond_packet = assembl_packet(&rps_header_r, &ip_header_r, (uint8_t *)data, strlen(data));
    rpsp_send(sockfd, respond_packet, sizeof(ip_header) + sizeof(rps_header) + strlen(data), dest_ip);
    free(respond_packet);
}
void send_taunt(int sockfd, ip_header ip_header_r, int session_id, const char *dest_ip) {
    char taunt[256];
    printf("Enter your taunt: ");
    scanf(" %255[^\n]", taunt);
    rps_header rps_header_r= create_rps_header(RPSP_TAUNT, 0, session_id, 0);
    uint8_t *respond_packet = assembl_packet(&rps_header_r, &ip_header_r, (uint8_t *)taunt, strlen(taunt));
    rpsp_send(sockfd, respond_packet, sizeof(ip_header) + sizeof(rps_header) + strlen(taunt), dest_ip);
    free(respond_packet);
}
