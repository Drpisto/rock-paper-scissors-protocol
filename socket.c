#include "rps.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdio.h>


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


ssize_t rpsp_recv(int sockfd, uint8_t *buffer, size_t buffer_len,
                  char *src_ip, size_t src_ip_len) {
    ssize_t received = recv(sockfd, buffer, buffer_len, 0);
    if (received < 0) {
        perror("recv");
        return -1;
    }

    
    struct iphdr *ip = (struct iphdr *)buffer;
    uint8_t      *rpsp = buffer + (ip->ihl * 4);

    
    if (rpsp[0] != RPSP_MAGIC) {
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