#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/time.h>

static double timeout_func(const struct timeval *start, const struct timeval *end) {
    double secDiff = (double)(end->tv_sec - start->tv_sec);
    double usecDiff = (double)(end->tv_usec - start->tv_usec);
    return secDiff * 1000.0 + usecDiff / 1000.0;
}

int main(int argc, char *argv[]) {

    const char *host = argv[1];
    const char *port_str = argv[2];

    char *endptr = NULL;
    long port = strtol(port_str, &endptr, 10);

    struct addrinfo hints;
    struct addrinfo *server_info = NULL;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    int status = getaddrinfo(host, port_str, &hints, &server_info);

    char server_host_ip[NI_MAXHOST];
    if (getnameinfo(server_info->ai_addr, server_info->ai_addrlen,
                    server_host_ip, sizeof(server_host_ip), NULL, 0, NI_NUMERICHOST) != 0) {
        strncpy(server_host_ip, host, sizeof(server_host_ip));
        server_host_ip[sizeof(server_host_ip) - 1] = '\0';
    }

    int sockfd = socket(server_info->ai_family, server_info->ai_socktype, server_info->ai_protocol);

    // Set socket receive timeout to 1 second from https://stackoverflow.com/questions/13547721/udp-socket-set-timeout
    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        perror("Error");
        close(sockfd);
        freeaddrinfo(server_info);
        return 1;
    }

    int transmitted = 0;
    int received = 0;
    double rtt_min = 0.0;
    double rtt_max = 0.0;
    double rtt_sum = 0.0;

    //set buffers
    char send_buffer[1024];
    char recv_buffer[1024];

    //loop to send 10 pings
    for (int seq = 1; seq <= 10; ++seq) {
        struct timeval send_time;
        gettimeofday(&send_time, NULL);

        int msg_len = snprintf(send_buffer, sizeof(send_buffer), "PING %d %ld.%06ld", seq,
                               (long)send_time.tv_sec, (long)send_time.tv_usec);
        if (msg_len < 0 || msg_len >= (int)sizeof(send_buffer)) {
            fprintf(stderr, "Message formatting error\n");
            break;
        }

        int sent = sendto(sockfd, send_buffer, msg_len, 0, server_info->ai_addr, server_info->ai_addrlen);
        if (sent < 0) {
            perror("sendto");
            break;
        }
        ++transmitted;

        struct sockaddr_storage reply_addr;
        socklen_t reply_addr_len = sizeof(reply_addr);

        int received_bytes = recvfrom(sockfd, recv_buffer, sizeof(recv_buffer) - 1, 0,
                                          (struct sockaddr *)&reply_addr, &reply_addr_len);

        if (received_bytes < 0) {
            printf("Request timeout for seq#=%d\n", seq);
        } else {
            struct timeval recv_time;
            gettimeofday(&recv_time, NULL);

            recv_buffer[received_bytes] = '\0';
            double rtt = timeout_func(&send_time, &recv_time);

            if (received == 0 || rtt < rtt_min) {
                rtt_min = rtt;
            }
            if (received == 0 || rtt > rtt_max) {
                rtt_max = rtt;
            }
            rtt_sum += rtt;
            ++received;

            char host_buffer[NI_MAXHOST];
            if (getnameinfo((struct sockaddr *)&reply_addr, reply_addr_len,
                            host_buffer, sizeof(host_buffer), NULL, 0, NI_NUMERICHOST) != 0) {
                strncpy(host_buffer, "unknown", sizeof(host_buffer));
                host_buffer[sizeof(host_buffer) - 1] = '\0';
            }

            printf("PING received from %s: seq#=%d time=%.3f ms\n", host_buffer, seq, rtt);
        }

        if (seq != 10) {
            sleep(1);
        }
    }

    double loss_percent = 0.0;

    printf("--- %s ping statistics ---\n", server_host_ip);
    
    if (transmitted > 0) {
        loss_percent = ((double)(transmitted - received) / transmitted) * 100.0;
    }

    double min_rtt;
    double max_rtt;
    double avg_rtt;

    if (received > 0) {
        min_rtt = rtt_min;
        max_rtt = rtt_max;
        avg_rtt = rtt_sum / received;
    } else {
        min_rtt = 0.0;
        max_rtt = 0.0;
        avg_rtt = 0.0;
    }

    
    if(received > 0) {
        printf("%d packets transmitted, %d received, %.0f%% packet loss", transmitted, received, loss_percent);
        printf(" rtt min/avg/max = %.3f %.3f %.3f ms\n",min_rtt, avg_rtt, max_rtt);
    } else {
        printf("%d packets transmitted, %d received, %.0f%% packet loss\n", transmitted, received, loss_percent);
    }
    freeaddrinfo(server_info);
    close(sockfd);
    return 0;
}
