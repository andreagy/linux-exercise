#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <sys/select.h>
#include <sys/timerfd.h>
#include <stdint.h>

#define MAX_VAL_LEN 32
#define BUFFER_SIZE 256
#define PORT1 4001
#define PORT2 4002
#define PORT3 4003
#define TIMER_INTERVAL_MS 100

int connect_tcp(int port) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        exit(1);
    }
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);
    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect");
        exit(1);
    }
    // Get the current flags of the socket and set it to non-blocking mode
    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);

    return sock;
}

int create_timer(uint64_t interval_ms) {
    int timerfd = timerfd_create(CLOCK_MONOTONIC, 0);
    if (timerfd < 0) {
        perror("timerfd_create");
        exit(1);
    }

    struct itimerspec spec;
    // Initial expiration
    spec.it_value.tv_sec = interval_ms / 1000;
    spec.it_value.tv_nsec = (interval_ms % 1000) * 1000000;
    // Repeat interval
    spec.it_interval.tv_sec = interval_ms / 1000;
    spec.it_interval.tv_nsec = (interval_ms % 1000) * 1000000;

    if (timerfd_settime(timerfd, 0, &spec, NULL) < 0) {
        perror("timerfd_settime");
        exit(1);
    }

    return timerfd;
}

uint64_t get_timestamp_ms() {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (uint64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

int main() {
    int sockets[3];
    sockets[0] = connect_tcp(PORT1);
    sockets[1] = connect_tcp(PORT2);
    sockets[2] = connect_tcp(PORT3);

    int timer_fd = create_timer(TIMER_INTERVAL_MS);

    char latest_value[3][MAX_VAL_LEN];
    strcpy(latest_value[0], "--");
    strcpy(latest_value[1], "--");
    strcpy(latest_value[2], "--");

    char buffer[3][BUFFER_SIZE];
    int buffer_len[3] = {0, 0, 0};

    while (1) {
        fd_set readfds;
        FD_ZERO(&readfds);
        // Add all socket file descriptors
        for (int i = 0; i < 3; i++) {
            FD_SET(sockets[i], &readfds);
        }
        // Add the timer file descriptor
        FD_SET(timer_fd, &readfds);

        // Keep track of the maximum file descriptor for select()
        int maxfd = timer_fd;
        for (int i = 0; i < 3; i++) {
            if (sockets[i] > maxfd) {
                maxfd = sockets[i];
            }
        }

        int ready = select(maxfd + 1, &readfds, NULL, NULL, NULL); // timer will wake up select() every 100ms
        if (ready < 0) {
            perror("select()");
            exit(1);
        }
        
        // Read data from each socket if available
        for (int i = 0; i < 3; i++) {
            if (FD_ISSET(sockets[i], &readfds)) {
                char temp_buffer[BUFFER_SIZE];
                int bytes_read = read(sockets[i], temp_buffer, BUFFER_SIZE - 1);
                if (bytes_read > 0) {
                    if (buffer_len[i] + bytes_read < BUFFER_SIZE) {
                        memcpy(buffer[i] + buffer_len[i], temp_buffer, bytes_read);
                        buffer_len[i] += bytes_read;
                        buffer[i][buffer_len[i]] = '\0'; // Null-terminate the buffer

                        // Process complete lines in the buffer
                        char *line_start = buffer[i];
                        char *newline_pos;
                        while ((newline_pos = memchr(line_start, '\n', buffer_len[i] - (line_start - buffer[i]))) != NULL) {
                            *newline_pos = '\0'; // Replace newline with null terminator
                            strncpy(latest_value[i], line_start, MAX_VAL_LEN - 1);
                            latest_value[i][MAX_VAL_LEN - 1] = '\0'; // Add null termination
                            line_start = newline_pos + 1; // Move to the start of the next line
                        }

                        // Move any remaining data to the start of the buffer
                        int remaining_len = buffer_len[i] - (line_start - buffer[i]);
                        memmove(buffer[i], line_start, remaining_len);
                        buffer_len[i] = remaining_len;
                    }
                }   
            }
        }

        // Check if the timer has expired
        if (FD_ISSET(timer_fd, &readfds)) {
            uint64_t timer_expirations;
            if (read(timer_fd, &timer_expirations, sizeof(timer_expirations)) == -1) {
                perror("read timer_fd");
                exit(1);
            }

            // Print JSON output
            uint64_t timestamp = get_timestamp_ms();
            printf("{\"timestamp\": %lu, \"out1\": \"%s\", \"out2\": \"%s\", \"out3\": \"%s\"}\n",
                   timestamp, latest_value[0], latest_value[1], latest_value[2]);

            // Reset values to "--"
            strcpy(latest_value[0], "--");
            strcpy(latest_value[1], "--");
            strcpy(latest_value[2], "--");
        }
    }

    close(timer_fd);
    for (int i = 0; i < 3; i++) {
        close(sockets[i]);
    }

    return 0;

}