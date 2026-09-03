#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdint.h>

#define CONTROL_CHANNEL 4000

void send_read(int sock, uint16_t object, uint16_t property) {
    uint16_t msg[3];
    msg[0] = htons(1);
    msg[1] = htons(object);
    msg[2] = htons(property);

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(CONTROL_CHANNEL);
    inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);

    ssize_t sent = sendto(sock, msg, sizeof(msg), 0,
                          (struct sockaddr*)&server_addr, sizeof(server_addr));
    if (sent < 0) {
        perror("sendto");
    } else {
        printf("Sent read: object=%u, property=%u\n", object, property);
    }
}

int main() {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("socket");
        exit(1);
    }

    for (uint16_t obj = 1; obj <= 3; obj++) {
        for (uint16_t prop = 0; prop <= 100; prop++) {
            send_read(sock, obj, prop);
            usleep(150000);
        }
        printf("Done scanning object %u\n", obj);
    }

    close(sock);
    return 0;
}