#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <arpa/inet.h>
#include <netdb.h>

#define DEFAULT_COUNT    4
#define DEFAULT_PAYLOAD  56   /* bytes of ICMP payload (classic ping default) */
#define RECV_TIMEOUT_S   2

static volatile int g_stop   = 0;
static int          g_sent   = 0;
static int          g_recv   = 0;

static void sig_int(int s) { (void)s; g_stop = 1; }

static uint16_t icmp_checksum(void *data, size_t len) {
    uint16_t *p = (uint16_t *)data;
    uint32_t  sum = 0;
    while (len > 1) { sum += *p++; len -= 2; }
    if (len)         sum += *(uint8_t *)p;
    while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
    return (uint16_t)~sum;
}

static double timespec_diff_ms(const struct timespec *a, const struct timespec *b) {
    return (double)(b->tv_sec  - a->tv_sec)  * 1000.0
         + (double)(b->tv_nsec - a->tv_nsec) / 1e6;
}

int ping_main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: ping [-c count] [-s size] [-i interval] <host>\n");
        return 1;
    }

    int count        = DEFAULT_COUNT;
    int payload_size = DEFAULT_PAYLOAD;
    int interval_ms  = 1000;
    const char *host = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-c") == 0 && i + 1 < argc)
            count = atoi(argv[++i]);
        else if (strcmp(argv[i], "-s") == 0 && i + 1 < argc)
            payload_size = atoi(argv[++i]);
        else if (strcmp(argv[i], "-i") == 0 && i + 1 < argc)
            interval_ms = (int)(atof(argv[++i]) * 1000);
        else if (argv[i][0] != '-')
            host = argv[i];
    }

    if (!host) {
        fprintf(stderr, "ping: missing host\n");
        return 1;
    }

    /* Resolve the target hostname */
    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    int err = getaddrinfo(host, NULL, &hints, &res);
    if (err != 0) {
        fprintf(stderr, "ping: %s: %s\n", host, gai_strerror(err));
        return 1;
    }
    struct sockaddr_in dst;
    memcpy(&dst, res->ai_addr, sizeof(dst));
    freeaddrinfo(res);

    char dst_str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &dst.sin_addr, dst_str, sizeof(dst_str));
    printf("PING %s (%s): %d data bytes\n", host, dst_str, payload_size);

    /* Raw ICMP socket — requires root or CAP_NET_RAW */
    int fd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (fd < 0) {
        perror("socket (ping requires root or CAP_NET_RAW)");
        return 1;
    }

    struct timeval tv = { RECV_TIMEOUT_S, 0 };
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    signal(SIGINT, sig_int);

    int pkt_len = (int)sizeof(struct icmphdr) + payload_size;
    uint8_t *pkt = calloc(1, (size_t)pkt_len);
    uint8_t  rbuf[1500];

    uint16_t pid = (uint16_t)getpid();
    double rtt_min = 1e9, rtt_max = 0.0, rtt_sum = 0.0;

    for (int seq = 1; seq <= count && !g_stop; seq++) {
        struct icmphdr *icmp = (struct icmphdr *)pkt;
        icmp->type             = ICMP_ECHO;
        icmp->code             = 0;
        icmp->un.echo.id       = pid;
        icmp->un.echo.sequence = (uint16_t)seq;
        icmp->checksum         = 0;
        for (int j = 0; j < payload_size; j++)
            pkt[sizeof(struct icmphdr) + j] = (uint8_t)(j & 0xFF);
        icmp->checksum = icmp_checksum(pkt, (size_t)pkt_len);

        struct timespec t0, t1;
        clock_gettime(CLOCK_MONOTONIC, &t0);

        if (sendto(fd, pkt, (size_t)pkt_len, 0,
                   (struct sockaddr *)&dst, sizeof(dst)) < 0) {
            perror("sendto"); break;
        }
        g_sent++;

        /* Receive loop — discard unrelated packets */
        while (!g_stop) {
            struct sockaddr_in from;
            socklen_t fromlen = sizeof(from);
            ssize_t n = recvfrom(fd, rbuf, sizeof(rbuf), 0,
                                 (struct sockaddr *)&from, &fromlen);
            if (n < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                    printf("Request timeout for icmp_seq %d\n", seq);
                else
                    perror("recvfrom");
                break;
            }
            clock_gettime(CLOCK_MONOTONIC, &t1);

            /* Skip IP header */
            struct iphdr *iph = (struct iphdr *)rbuf;
            int ip_hlen = iph->ihl * 4;
            if (n < ip_hlen + (ssize_t)sizeof(struct icmphdr)) continue;

            struct icmphdr *reply = (struct icmphdr *)(rbuf + ip_hlen);
            if (reply->type != ICMP_ECHOREPLY)         continue;
            if (reply->un.echo.id != pid)               continue;
            if (reply->un.echo.sequence != (uint16_t)seq) continue;

            double rtt = timespec_diff_ms(&t0, &t1);
            char from_str[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &from.sin_addr, from_str, sizeof(from_str));
            printf("%d bytes from %s: icmp_seq=%d ttl=%d time=%.3f ms\n",
                payload_size, from_str, seq, iph->ttl, rtt);

            if (rtt < rtt_min) rtt_min = rtt;
            if (rtt > rtt_max) rtt_max = rtt;
            rtt_sum += rtt;
            g_recv++;
            break;
        }

        if (seq < count && !g_stop)
            usleep((unsigned int)(interval_ms * 1000));
    }

    free(pkt);
    close(fd);

    int loss = g_sent > 0 ? (g_sent - g_recv) * 100 / g_sent : 0;
    printf("\n--- %s ping statistics ---\n", host);
    printf("%d packets transmitted, %d received, %d%% packet loss\n",
        g_sent, g_recv, loss);
    if (g_recv > 0)
        printf("rtt min/avg/max = %.3f/%.3f/%.3f ms\n",
            rtt_min, rtt_sum / g_recv, rtt_max);

    return (g_recv > 0) ? 0 : 1;
}
