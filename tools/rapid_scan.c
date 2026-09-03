#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdint.h>
#include <time.h>

int main() {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) { perror("socket"); return 1; }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(4000);
    inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);

    uint16_t msg[4];
    msg[0] = htons(2);      // read, write
    msg[1] = htons(1);      // object 1
    msg[3] = htons(50);      // value 

    for (uint32_t prop = 0; prop <= 65535; prop++) {
        msg[2] = htons((uint16_t)prop);
        sendto(sock, msg, sizeof(msg), 0, (struct sockaddr*)&server_addr, sizeof(server_addr));
        struct timespec ts = {0, 1000000};
        nanosleep(&ts, NULL);
    }

    close(sock);
    return 0;
}