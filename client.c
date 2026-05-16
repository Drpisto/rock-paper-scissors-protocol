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

void send_hello(int, int, const char*);
void send_move(int, int, const char*);
void send_taunt(int, int, const char*);
void send_data(int, int, const char*);

int rpsp_client(const char *dest_ip_str)
{
    int sockfd = rpsp_socket_open();
    if (sockfd < 0) return -1;

    printf("Welcome to Rock-Paper-Scissors Protocol Client!\n");
    printf("1. send hello\n");
    printf("2. send move\n");
    printf("3. send taunt\n");
    printf("4. send data\n");

    int session_id = rand() % 65536;
    int choice;

    printf("Enter your choice: ");
    scanf("%d", &choice);
    int ch;
    while ((ch = getchar()) != '\n' && ch != EOF);

    if (choice == 1) {
        send_hello(sockfd, session_id, dest_ip_str);
    } else if (choice == 2) {
        send_move(sockfd, session_id, dest_ip_str);
    } else if (choice == 3) {
        send_taunt(sockfd, session_id, dest_ip_str);
    } else if (choice == 4) {
        send_data(sockfd, session_id, dest_ip_str);
    } else {
        printf("Invalid choice.\n");
    }

    uint8_t buffer[65536];
    char src_ip[INET_ADDRSTRLEN];
    printf("Waiting for response...\n");
    
    struct timeval tv = {3, 0};
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    
    ssize_t result = rpsp_recv(sockfd, buffer, sizeof(buffer), src_ip, sizeof(src_ip));
    
    if (result <= 0) {
        printf("No response received\n");
        close(sockfd);
        return 1;
    }

    if (result > 0) {
        struct iphdr *ip = (struct iphdr *)buffer;
        rps_header *rpsp = (rps_header *)(buffer + (ip->ihl * 4));
        
        printf("DEBUG: got type=%d magic=0x%02X\n", rpsp->type, rpsp->magic);
        
        if (rpsp->type == RPSP_ACCEPT) {
            printf("Server accepted connection!\n");
        } else if (rpsp->type == RPSP_REJECT) {
            printf("Your connection was rejected by the server.\n");
        } else if (rpsp->type == RPSP_RESULT) {
            rpsp_move_t opponent_move = (rpsp->move_round >> 6) & 0x03;
            rpsp_result_t game_result = det_result(get_move_user(), opponent_move);
            printf("You %s!\n", game_result == RESULT_WIN ? "win you can send data" : game_result == RESULT_LOSE ? "lose try again" : "draw");
        } else if (rpsp->type == RPSP_FIN) {
            printf("Your connection was closed by the server.\n");
        } else if (rpsp->type == RPSP_RST) {
            printf("Your connection was reset by the server.\n");
        }
    }

    printf("Press Enter to exit...\n");
    fflush(stdout);
    int ch2;
    while ((ch2 = getchar()) != '\n' && ch2 != EOF);
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

void send_hello(int sockfd, int session_id, const char *dest_ip) {
    rps_header rps_header_r = create_rps_header(RPSP_HELLO, 0, session_id, 0);
    rpsp_send(sockfd, (uint8_t*)&rps_header_r, sizeof(rps_header), dest_ip);
}

void send_move(int sockfd, int session_id, const char *dest_ip) {
    rpsp_move_t my_move = get_move_user();
    my_move = (my_move << 6) & 0xC0;
    rps_header rps_header_r = create_rps_header(RPSP_MOVE, my_move, session_id, 0);
    rpsp_send(sockfd, (uint8_t*)&rps_header_r, sizeof(rps_header), dest_ip);
}

void send_data(int sockfd, int session_id, const char *dest_ip) {
    char data[256];
    printf("Enter your data: ");
    scanf(" %255[^\n]", data);
    size_t len = strlen(data);
    size_t total = sizeof(rps_header) + len;
    uint8_t *pkt = malloc(total);
    rps_header *h = (rps_header*)pkt;
    *h = create_rps_header(RPSP_DATA, 0, session_id, 0);
    memcpy(pkt + sizeof(rps_header), data, len);
    rpsp_send(sockfd, pkt, total, dest_ip);
    free(pkt);
}
void send_taunt(int sockfd, int session_id, const char *dest_ip) {
    char taunt[256];
    printf("Enter your taunt: ");
    scanf(" %255[^\n]", taunt);
    size_t len = strlen(taunt);
    size_t total = sizeof(rps_header) + len;
    uint8_t *pkt = malloc(total);
    rps_header *h = (rps_header*)pkt;
    *h = create_rps_header(RPSP_TAUNT, 0, session_id, 0);
    memcpy(pkt + sizeof(rps_header), taunt, len);
    rpsp_send(sockfd, pkt, total, dest_ip);
    free(pkt);
}
