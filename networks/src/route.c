#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <net/route.h>
#include <netinet/in.h>
#include <arpa/inet.h>

static void show_route_table(void) {
    FILE *f = fopen("/proc/net/route", "r");
    if (!f) { perror("/proc/net/route"); return; }

    printf("%-18s %-18s %-18s %-8s %-6s %s\n",
        "Destination", "Gateway", "Genmask", "Flags", "Metric", "Iface");
    printf("%-18s %-18s %-18s %-8s %-6s %s\n",
        "-------------------", "------------------", "------------------",
        "--------", "------", "-----");

    char line[256];
    fgets(line, sizeof(line), f); /* skip header */

    while (fgets(line, sizeof(line), f)) {
        char iface[IFNAMSIZ];
        unsigned long dest, gw, mask, flags, metric, refcnt, use;

        if (sscanf(line, "%15s %lx %lx %lx %lu %lu %lu %lx",
                   iface, &dest, &gw, &flags, &refcnt, &use, &metric, &mask) != 8)
            continue;

        struct in_addr d, g, m;
        d.s_addr = (uint32_t)dest;
        g.s_addr = (uint32_t)gw;
        m.s_addr = (uint32_t)mask;

        char dst_str[INET_ADDRSTRLEN], gw_str[INET_ADDRSTRLEN], mask_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &d, dst_str,  sizeof(dst_str));
        inet_ntop(AF_INET, &g, gw_str,   sizeof(gw_str));
        inet_ntop(AF_INET, &m, mask_str, sizeof(mask_str));

        char flagstr[8] = "----";
        if (flags & RTF_UP)      flagstr[0] = 'U';
        if (flags & RTF_GATEWAY) flagstr[1] = 'G';
        if (flags & RTF_HOST)    flagstr[2] = 'H';
        if (flags & RTF_REJECT)  flagstr[3] = '!';

        printf("%-18s %-18s %-18s %-8s %-6lu %s\n",
            dst_str, gw_str, mask_str, flagstr, metric, iface);
    }
    fclose(f);
}

static int route_modify(int argc, char **argv, int add) {
    if (argc < 2) {
        fprintf(stderr,
            "Usage: route %s <destination|default> [gw <gateway>] "
            "[netmask <mask>] [dev <iface>]\n",
            add ? "add" : "del");
        return 1;
    }

    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) { perror("socket"); return 1; }

    struct rtentry rt;
    memset(&rt, 0, sizeof(rt));

    struct sockaddr_in *dst  = (struct sockaddr_in *)&rt.rt_dst;
    struct sockaddr_in *gw   = (struct sockaddr_in *)&rt.rt_gateway;
    struct sockaddr_in *mask = (struct sockaddr_in *)&rt.rt_genmask;

    dst->sin_family  = AF_INET;
    gw->sin_family   = AF_INET;
    mask->sin_family = AF_INET;

    rt.rt_flags = RTF_UP;

    if (strcmp(argv[1], "default") == 0) {
        dst->sin_addr.s_addr  = INADDR_ANY;
        mask->sin_addr.s_addr = INADDR_ANY;
        rt.rt_flags |= RTF_GATEWAY;
    } else {
        if (inet_pton(AF_INET, argv[1], &dst->sin_addr) != 1) {
            fprintf(stderr, "route: invalid destination '%s'\n", argv[1]);
            close(fd); return 1;
        }
        mask->sin_addr.s_addr = 0xFFFFFFFF; /* default: host route */
    }

    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "gw") == 0 && i + 1 < argc) {
            if (inet_pton(AF_INET, argv[++i], &gw->sin_addr) != 1) {
                fprintf(stderr, "route: invalid gateway '%s'\n", argv[i]);
                close(fd); return 1;
            }
            rt.rt_flags |= RTF_GATEWAY;
        } else if (strcmp(argv[i], "netmask") == 0 && i + 1 < argc) {
            if (inet_pton(AF_INET, argv[++i], &mask->sin_addr) != 1) {
                fprintf(stderr, "route: invalid netmask '%s'\n", argv[i]);
                close(fd); return 1;
            }
        } else if (strcmp(argv[i], "dev") == 0 && i + 1 < argc) {
            rt.rt_dev = argv[++i];
        } else {
            fprintf(stderr, "route: unknown option '%s'\n", argv[i]);
            close(fd); return 1;
        }
    }

    if (ioctl(fd, add ? SIOCADDRT : SIOCDELRT, &rt) != 0) {
        perror(add ? "SIOCADDRT" : "SIOCDELRT");
        close(fd); return 1;
    }

    close(fd);
    return 0;
}

int route_main(int argc, char **argv) {
    if (argc < 2) {
        show_route_table();
        return 0;
    }

    if (strcmp(argv[1], "add") == 0)
        return route_modify(argc - 1, argv + 1, 1);
    if (strcmp(argv[1], "del") == 0 || strcmp(argv[1], "delete") == 0)
        return route_modify(argc - 1, argv + 1, 0);
    if (strcmp(argv[1], "show") == 0 || strcmp(argv[1], "-n") == 0) {
        show_route_table();
        return 0;
    }

    fprintf(stderr, "Usage: route [show|add|del] ...\n");
    return 1;
}
