#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>

static void print_help(void) {
    printf("Usage: dns <hostname|ip> [hostname|ip ...]\n");
    printf("  Performs forward lookups for hostnames and reverse lookups for IPs.\n");
}

static int lookup_one(const char *target) {
    /* Reverse lookup if it parses as an IPv4 or IPv6 address */
    struct in_addr  v4;
    struct in6_addr v6;

    if (inet_pton(AF_INET, target, &v4) == 1) {
        struct sockaddr_in sa;
        memset(&sa, 0, sizeof(sa));
        sa.sin_family = AF_INET;
        sa.sin_addr   = v4;
        char host[NI_MAXHOST];
        int r = getnameinfo((struct sockaddr *)&sa, sizeof(sa),
                            host, sizeof(host), NULL, 0, NI_NAMEREQD);
        if (r != 0) {
            fprintf(stderr, "dns: reverse lookup for %s failed: %s\n",
                    target, gai_strerror(r));
            return 1;
        }
        printf("%s  ->  %s\n", target, host);
        return 0;
    }

    if (inet_pton(AF_INET6, target, &v6) == 1) {
        struct sockaddr_in6 sa6;
        memset(&sa6, 0, sizeof(sa6));
        sa6.sin6_family = AF_INET6;
        sa6.sin6_addr   = v6;
        char host[NI_MAXHOST];
        int r = getnameinfo((struct sockaddr *)&sa6, sizeof(sa6),
                            host, sizeof(host), NULL, 0, NI_NAMEREQD);
        if (r != 0) {
            fprintf(stderr, "dns: reverse lookup for %s failed: %s\n",
                    target, gai_strerror(r));
            return 1;
        }
        printf("%s  ->  %s\n", target, host);
        return 0;
    }

    /* Forward lookup */
    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    int err = getaddrinfo(target, NULL, &hints, &res);
    if (err != 0) {
        fprintf(stderr, "dns: %s: %s\n", target, gai_strerror(err));
        return 1;
    }

    printf("%s:\n", target);
    for (struct addrinfo *r = res; r != NULL; r = r->ai_next) {
        char ipbuf[INET6_ADDRSTRLEN];
        void *addr_ptr;
        const char *family;

        if (r->ai_family == AF_INET) {
            addr_ptr = &((struct sockaddr_in *)r->ai_addr)->sin_addr;
            family   = "IPv4";
        } else if (r->ai_family == AF_INET6) {
            addr_ptr = &((struct sockaddr_in6 *)r->ai_addr)->sin6_addr;
            family   = "IPv6";
        } else {
            continue;
        }

        inet_ntop(r->ai_family, addr_ptr, ipbuf, sizeof(ipbuf));
        printf("  %-6s %s\n", family, ipbuf);
    }
    freeaddrinfo(res);
    return 0;
}

int dns_main(int argc, char **argv) {
    if (argc < 2) {
        print_help();
        return 1;
    }

    if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
        print_help();
        return 0;
    }

    int ret = 0;
    for (int i = 1; i < argc; i++) {
        if (lookup_one(argv[i]) != 0)
            ret = 1;
        if (i + 1 < argc) printf("\n");
    }
    return ret;
}
