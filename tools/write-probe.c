#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdint.h>

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <object> <property> <value>\n", argv[0]);
        return 1;
    }
    uint16_t obj = (uint16_t)atoi(argv[1]);
    uint16_t prop = (uint16_t)atoi(argv[2]);
    uint16_t val = (uint16_t)atoi(argv[3]);

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) { perror("socket"); return 1; }

    uint16_t msg[4];
    msg[0] = htons(2);
    msg[1] = htons(obj);
    msg[2] = htons(prop);
    msg[3] = htons(val);

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(4000);
    inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);

    if (sendto(sock, msg, sizeof(msg), 0,
               (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("sendto");
        return 1;
    }

    printf("Sent write: object=%u property=%u value=%u\n", obj, prop, val);
    close(sock);
    return 0;
}