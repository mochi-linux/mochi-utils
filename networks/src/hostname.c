#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <netdb.h>
#include <sys/socket.h>

#define MAX_HOSTNAME 253  /* RFC 1035 */

int hostname_main(int argc, char **argv) {
    if (argc == 1) {
        char buf[MAX_HOSTNAME + 1];
        if (gethostname(buf, sizeof(buf)) != 0) {
            perror("gethostname");
            return 1;
        }
        printf("%s\n", buf);
        return 0;
    }

    if (argc == 2) {
        if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
            printf("Usage: hostname [NAME]\n");
            printf("  Without arguments, print the current hostname.\n");
            printf("  With NAME, set the hostname (requires root).\n");
            return 0;
        }

        if (strcmp(argv[1], "-f") == 0 || strcmp(argv[1], "--fqdn") == 0) {
            char buf[MAX_HOSTNAME + 1];
            if (gethostname(buf, sizeof(buf)) != 0) {
                perror("gethostname");
                return 1;
            }
            /* getaddrinfo-based FQDN resolution */
            struct addrinfo *res, hints = {0};
            hints.ai_flags = AI_CANONNAME;
            if (getaddrinfo(buf, NULL, &hints, &res) == 0) {
                printf("%s\n", res->ai_canonname ? res->ai_canonname : buf);
                freeaddrinfo(res);
            } else {
                printf("%s\n", buf);
            }
            return 0;
        }

        size_t len = strlen(argv[1]);
        if (len == 0 || len > MAX_HOSTNAME) {
            fprintf(stderr, "hostname: name too long (max %d)\n", MAX_HOSTNAME);
            return 1;
        }
        if (sethostname(argv[1], len) != 0) {
            perror("sethostname");
            return 1;
        }
        return 0;
    }

    fprintf(stderr, "Usage: hostname [-f] [NAME]\n");
    return 1;
}
