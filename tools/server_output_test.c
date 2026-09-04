#include <arpa/inet.h>
#include <errno.h>
#include <math.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#define DATA_PORT 4001
#define OUTPUT_COUNT 3
#define CONTROL_PORT 4000
#define WRITE_OP 2
#define OUT1_OBJ 1
#define FREQ_PROP 255
#define AMP_PROP 170
#define BUFFER_SIZE 256
#define MAX_SAMPLES 100000

struct sample {
    double time;
    double value;
};

static double monotonic_seconds(void) {
    struct timespec timestamp;
    clock_gettime(CLOCK_MONOTONIC, &timestamp);
    return timestamp.tv_sec + timestamp.tv_nsec / 1000000000.0;
}

static int connect_output(int output_number) {
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in address;

    if (socket_fd < 0) {
        perror("socket");
        return -1;
    }

    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(DATA_PORT + output_number - 1);
    inet_pton(AF_INET, "127.0.0.1", &address.sin_addr);

    if (connect(socket_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        fprintf(stderr, "connect output %d: %s\n", output_number,
                strerror(errno));
        close(socket_fd);
        return -1;
    }

    return socket_fd;
}

static int send_write(uint16_t property, uint16_t value) {
    int socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
    uint16_t message[4] = {
        htons(WRITE_OP), htons(OUT1_OBJ), htons(property), htons(value)
    };
    struct sockaddr_in address;
    ssize_t sent;

    if (socket_fd < 0) {
        perror("socket");
        return -1;
    }

    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(CONTROL_PORT);
    inet_pton(AF_INET, "127.0.0.1", &address.sin_addr);
    sent = sendto(socket_fd, message, sizeof(message), 0,
                  (struct sockaddr *)&address, sizeof(address));
    close(socket_fd);

    if (sent != (ssize_t)sizeof(message)) {
        perror("sendto");
        return -1;
    }

    return 0;
}

static void print_header(int observe_only, int output_number,
                         uint16_t frequency_value, uint16_t amplitude_value) {
    if (observe_only) {
        printf("mode: observation only (output %d)\n", output_number);
    } else {
        printf("frequency property: %u\n", frequency_value);
        printf("amplitude property: %u\n", amplitude_value);
    }
}

static void report_output3(const struct sample *samples, size_t sample_count,
                           double minimum, double maximum) {
    double midpoint = (minimum + maximum) / 2.0;
    double previous_transition = 0.0;
    double previous_rising = 0.0;
    double high_duration_sum = 0.0;
    double low_duration_sum = 0.0;
    double period_sum = 0.0;
    size_t rising_count = 0;
    size_t falling_count = 0;
    size_t high_duration_count = 0;
    size_t low_duration_count = 0;
    int high = samples[0].value >= midpoint;

    for (size_t index = 1; index < sample_count; index++) {
        int current_high = samples[index].value >= midpoint;

        if (current_high == high) {
            continue;
        }

        double transition_time = samples[index].time;
        if (samples[index].value != samples[index - 1].value) {
            double fraction = (midpoint - samples[index - 1].value) /
                              (samples[index].value - samples[index - 1].value);
            transition_time = samples[index - 1].time +
                              fraction * (samples[index].time - samples[index - 1].time);
        }

        if (current_high) {
            if (rising_count > 0) {
                period_sum += transition_time - previous_rising;
            }
            if (falling_count > 0) {
                low_duration_sum += transition_time - previous_transition;
                low_duration_count++;
            }
            previous_rising = transition_time;
            rising_count++;
        } else {
            if (rising_count > 0) {
                high_duration_sum += transition_time - previous_transition;
                high_duration_count++;
            }
            falling_count++;
        }

        previous_transition = transition_time;
        high = current_high;
    }

    printf("midpoint: %.6f\n", midpoint);
    printf("rising transitions: %zu\n", rising_count);
    printf("falling transitions: %zu\n", falling_count);
    if (rising_count > 1) {
        printf("average period: %.6f s\n",
               period_sum / (double)(rising_count - 1));
        printf("average frequency: %.6f Hz\n",
               (double)(rising_count - 1) / period_sum);
    } else {
        printf("average frequency: insufficient rising transitions\n");
    }
    if (high_duration_count > 0) {
        printf("average high duration: %.6f s\n",
               high_duration_sum / (double)high_duration_count);
    }
    if (low_duration_count > 0) {
        printf("average low duration: %.6f s\n",
               low_duration_sum / (double)low_duration_count);
    }
}

static void report_midpoint_frequency(const struct sample *samples,
                                      size_t sample_count, double midpoint) {
    double period_sum = 0.0;
    double previous_crossing = 0.0;
    size_t crossing_count = 0;

    for (size_t index = 1; index < sample_count; index++) {
        if (samples[index - 1].value < midpoint &&
            samples[index].value >= midpoint) {
            double fraction = (midpoint - samples[index - 1].value) /
                              (samples[index].value - samples[index - 1].value);
            double crossing_time = samples[index - 1].time +
                                   fraction * (samples[index].time - samples[index - 1].time);

            if (crossing_count > 0) {
                period_sum += crossing_time - previous_crossing;
            }
            previous_crossing = crossing_time;
            crossing_count++;
        }
    }

    if (crossing_count > 1) {
        printf("rising midpoint crossings: %zu\n", crossing_count);
        printf("estimated frequency: %.6f Hz\n",
               (double)(crossing_count - 1) / period_sum);
    } else {
        printf("estimated frequency: insufficient midpoint crossings\n");
    }
}

static void report_results(const struct sample *samples, size_t sample_count,
                           int observe_only, int output_number,
                           uint16_t frequency_value,
                           uint16_t amplitude_value) {
    double minimum = samples[0].value;
    double maximum = samples[0].value;
    double sum = 0.0;
    double midpoint;

    for (size_t index = 0; index < sample_count; index++) {
        if (samples[index].value < minimum) {
            minimum = samples[index].value;
        }
        if (samples[index].value > maximum) {
            maximum = samples[index].value;
        }
        sum += samples[index].value;
    }

    print_header(observe_only, output_number, frequency_value, amplitude_value);
    printf("samples: %zu\n", sample_count);
    printf("minimum: %.6f\n", minimum);
    printf("maximum: %.6f\n", maximum);
    printf("mean: %.6f\n", sum / sample_count);
    printf("observed peak-to-peak: %.6f\n", maximum - minimum);

    if (output_number == 3) {
        report_output3(samples, sample_count, minimum, maximum);
    } else {
        midpoint = (minimum + maximum) / 2.0;
        printf("midpoint: %.6f\n", midpoint);
        report_midpoint_frequency(samples, sample_count, midpoint);
    }
}

int main(int argc, char **argv) {
    int observe_only = argc > 1 && strcmp(argv[1], "observe") == 0;
    int output_number = 1;
    uint16_t frequency_value = 0;
    uint16_t amplitude_value = 0;
    double duration;
    int socket_fd;
    char buffer[BUFFER_SIZE];
    size_t buffer_length = 0;
    struct sample *samples;
    size_t sample_count = 0;
    double end_time;

    if (observe_only) {
        if (argc != 4) {
            fprintf(stderr, "usage: %s observe <output-number> <seconds>\n", argv[0]);
            return 2;
        }
        output_number = (int)strtol(argv[2], NULL, 10);
        duration = strtod(argv[3], NULL);
        if (output_number < 1 || output_number > OUTPUT_COUNT) {
            fprintf(stderr, "output number must be 1, 2, or 3\n");
            return 2;
        }
    } else {
        if (argc != 4) {
            fprintf(stderr, "usage: %s <frequency-property> <amplitude-property> <seconds>\n", argv[0]);
            return 2;
        }

        frequency_value = (uint16_t)strtoul(argv[1], NULL, 10);
        amplitude_value = (uint16_t)strtoul(argv[2], NULL, 10);
        duration = strtod(argv[3], NULL);
        if (frequency_value < 50 || frequency_value > 2000 ||
            amplitude_value < 1000 || amplitude_value > 10000) {
            fprintf(stderr, "values outside the discovered ranges\n");
            return 2;
        }
    }

    if (duration <= 0.0) {
        fprintf(stderr, "duration must be greater than zero\n");
        return 2;
    }

    if (!observe_only &&
        (send_write(FREQ_PROP, frequency_value) < 0 ||
         send_write(AMP_PROP, amplitude_value) < 0)) {
        return 1;
    }

    socket_fd = connect_output(output_number);
    if (socket_fd < 0) {
        return 1;
    }

    samples = calloc(MAX_SAMPLES, sizeof(*samples));
    if (samples == NULL) {
        perror("calloc");
        close(socket_fd);
        return 1;
    }

    end_time = monotonic_seconds() + duration;
    while (monotonic_seconds() < end_time && sample_count < MAX_SAMPLES) {
        fd_set read_set;
        struct timeval timeout = {0, 100000};
        int ready;

        FD_ZERO(&read_set);
        FD_SET(socket_fd, &read_set);
        ready = select(socket_fd + 1, &read_set, NULL, NULL, &timeout);
        if (ready < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("select");
            break;
        }
        if (ready == 0) {
            continue;
        }

        ssize_t bytes_read = read(socket_fd, buffer + buffer_length,
                                  sizeof(buffer) - buffer_length - 1);
        if (bytes_read <= 0) {
            if (bytes_read < 0 && errno == EINTR) {
                continue;
            }
            break;
        }
        buffer_length += (size_t)bytes_read;
        buffer[buffer_length] = '\0';

        char *line_start = buffer;
        char *newline;
        while ((newline = memchr(line_start, '\n',
                                 buffer + buffer_length - line_start)) != NULL) {
            *newline = '\0';
            if (sample_count < MAX_SAMPLES) {
                char *end_pointer;
                double value = strtod(line_start, &end_pointer);
                if (end_pointer != line_start && *end_pointer == '\0') {
                    samples[sample_count].time = monotonic_seconds();
                    samples[sample_count].value = value;
                    sample_count++;
                }
            }
            line_start = newline + 1;
        }

        buffer_length = (size_t)(buffer + buffer_length - line_start);
        memmove(buffer, line_start, buffer_length);
    }

    close(socket_fd);
    if (sample_count == 0) {
        fprintf(stderr, "no numeric samples received\n");
        free(samples);
        return 1;
    }

    report_results(samples, sample_count, observe_only, output_number,
                   frequency_value, amplitude_value);
    free(samples);
    return 0;
}