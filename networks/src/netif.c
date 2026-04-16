#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ifaddrs.h>

static int open_sock(void) {
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) perror("socket");
    return fd;
}

static void set_sin(struct sockaddr *sa, const char *ip) {
    struct sockaddr_in *sin = (struct sockaddr_in *)sa;
    memset(sin, 0, sizeof(*sin));
    sin->sin_family = AF_INET;
    inet_pton(AF_INET, ip, &sin->sin_addr);
}

static void print_hwaddr(const unsigned char *data, int len) {
    for (int i = 0; i < len; i++) {
        if (i > 0) printf(":");
        printf("%02x", data[i]);
    }
}

static void print_iface(const char *ifname) {
    int fd = open_sock();
    if (fd < 0) return;

    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, ifname, IFNAMSIZ - 1);

    /* Flags */
    short flags = 0;
    if (ioctl(fd, SIOCGIFFLAGS, &ifr) == 0)
        flags = ifr.ifr_flags;

    printf("%s: flags=%d<", ifname, (int)(unsigned short)flags);
    int first = 1;
#define FL(bit, label) \
    if (flags & (bit)) { if (!first) printf(","); printf(label); first = 0; }
    FL(IFF_UP,          "UP")
    FL(IFF_BROADCAST,   "BROADCAST")
    FL(IFF_LOOPBACK,    "LOOPBACK")
    FL(IFF_POINTOPOINT, "POINTOPOINT")
    FL(IFF_RUNNING,     "RUNNING")
    FL(IFF_NOARP,       "NOARP")
    FL(IFF_PROMISC,     "PROMISC")
    FL(IFF_MULTICAST,   "MULTICAST")
#undef FL
    printf(">");

    /* MTU */
    if (ioctl(fd, SIOCGIFMTU, &ifr) == 0)
        printf("  mtu %d", ifr.ifr_mtu);
    printf("\n");

    /* IPv4 address */
    if (ioctl(fd, SIOCGIFADDR, &ifr) == 0) {
        char ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr,
                  ip, sizeof(ip));
        printf("        inet %s", ip);

        if (ioctl(fd, SIOCGIFNETMASK, &ifr) == 0) {
            char mask[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &((struct sockaddr_in *)&ifr.ifr_netmask)->sin_addr,
                      mask, sizeof(mask));
            printf("  netmask %s", mask);
        }

        if ((flags & IFF_BROADCAST) && ioctl(fd, SIOCGIFBRDADDR, &ifr) == 0) {
            char bcast[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &((struct sockaddr_in *)&ifr.ifr_broadaddr)->sin_addr,
                      bcast, sizeof(bcast));
            printf("  broadcast %s", bcast);
        }
        printf("\n");
    }

    /* Hardware (MAC) address */
    if (ioctl(fd, SIOCGIFHWADDR, &ifr) == 0) {
        printf("        ether ");
        print_hwaddr((unsigned char *)ifr.ifr_hwaddr.sa_data, 6);
        printf("\n");
    }

    /* RX / TX statistics from /proc/net/dev */
    FILE *f = fopen("/proc/net/dev", "r");
    if (f) {
        char line[256];
        while (fgets(line, sizeof(line), f)) {
            char *colon = strchr(line, ':');
            if (!colon) continue;
            *colon = '\0';
            char *p = line;
            while (*p == ' ') p++;
            if (strcmp(p, ifname) != 0) continue;

            unsigned long rx_bytes, rx_pkts, rx_errs, rx_drop;
            unsigned long tx_bytes, tx_pkts, tx_errs, tx_drop;
            sscanf(colon + 1,
                "%lu %lu %lu %lu %*u %*u %*u %*u"
                " %lu %lu %lu %lu",
                &rx_bytes, &rx_pkts, &rx_errs, &rx_drop,
                &tx_bytes, &tx_pkts, &tx_errs, &tx_drop);
            printf("        RX packets %lu  bytes %lu  errors %lu  dropped %lu\n",
                rx_pkts, rx_bytes, rx_errs, rx_drop);
            printf("        TX packets %lu  bytes %lu  errors %lu  dropped %lu\n",
                tx_pkts, tx_bytes, tx_errs, tx_drop);
            break;
        }
        fclose(f);
    }
    printf("\n");
    close(fd);
}

int ifconfig_main(int argc, char **argv) {
    if (argc < 2) {
        /* List all interfaces (deduplicated) */
        struct ifaddrs *ifap;
        if (getifaddrs(&ifap) != 0) { perror("getifaddrs"); return 1; }

        char seen[64][IFNAMSIZ];
        int nseen = 0;
        for (struct ifaddrs *ifa = ifap; ifa; ifa = ifa->ifa_next) {
            int dup = 0;
            for (int i = 0; i < nseen; i++)
                if (strcmp(seen[i], ifa->ifa_name) == 0) { dup = 1; break; }
            if (!dup && nseen < 64)
                snprintf(seen[nseen++], IFNAMSIZ, "%s", ifa->ifa_name);
        }
        freeifaddrs(ifap);

        for (int i = 0; i < nseen; i++)
            print_iface(seen[i]);
        return 0;
    }

    const char *ifname = argv[1];

    if (argc == 2) {
        print_iface(ifname);
        return 0;
    }

    int fd = open_sock();
    if (fd < 0) return 1;

    struct ifreq ifr;
    int ret = 0;

    for (int i = 2; i < argc; i++) {
        memset(&ifr, 0, sizeof(ifr));
        strncpy(ifr.ifr_name, ifname, IFNAMSIZ - 1);

        if (strcmp(argv[i], "up") == 0) {
            if (ioctl(fd, SIOCGIFFLAGS, &ifr) == 0) {
                ifr.ifr_flags |= IFF_UP | IFF_RUNNING;
                if (ioctl(fd, SIOCSIFFLAGS, &ifr) != 0)
                    { perror("SIOCSIFFLAGS"); ret = 1; }
            }
        } else if (strcmp(argv[i], "down") == 0) {
            if (ioctl(fd, SIOCGIFFLAGS, &ifr) == 0) {
                ifr.ifr_flags &= ~(short)IFF_UP;
                if (ioctl(fd, SIOCSIFFLAGS, &ifr) != 0)
                    { perror("SIOCSIFFLAGS"); ret = 1; }
            }
        } else if (strcmp(argv[i], "promisc") == 0) {
            if (ioctl(fd, SIOCGIFFLAGS, &ifr) == 0) {
                ifr.ifr_flags |= IFF_PROMISC;
                if (ioctl(fd, SIOCSIFFLAGS, &ifr) != 0)
                    { perror("SIOCSIFFLAGS"); ret = 1; }
            }
        } else if (strcmp(argv[i], "-promisc") == 0) {
            if (ioctl(fd, SIOCGIFFLAGS, &ifr) == 0) {
                ifr.ifr_flags &= ~(short)IFF_PROMISC;
                if (ioctl(fd, SIOCSIFFLAGS, &ifr) != 0)
                    { perror("SIOCSIFFLAGS"); ret = 1; }
            }
        } else if (strcmp(argv[i], "mtu") == 0 && i + 1 < argc) {
            ifr.ifr_mtu = atoi(argv[++i]);
            if (ioctl(fd, SIOCSIFMTU, &ifr) != 0)
                { perror("SIOCSIFMTU"); ret = 1; }
        } else if (strcmp(argv[i], "netmask") == 0 && i + 1 < argc) {
            set_sin(&ifr.ifr_netmask, argv[++i]);
            if (ioctl(fd, SIOCSIFNETMASK, &ifr) != 0)
                { perror("SIOCSIFNETMASK"); ret = 1; }
        } else if (strcmp(argv[i], "broadcast") == 0 && i + 1 < argc) {
            set_sin(&ifr.ifr_broadaddr, argv[++i]);
            if (ioctl(fd, SIOCSIFBRDADDR, &ifr) != 0)
                { perror("SIOCSIFBRDADDR"); ret = 1; }
        } else {
            /* Try to interpret as an IPv4 address to assign */
            struct in_addr test;
            if (inet_pton(AF_INET, argv[i], &test) == 1) {
                set_sin(&ifr.ifr_addr, argv[i]);
                if (ioctl(fd, SIOCSIFADDR, &ifr) != 0)
                    { perror("SIOCSIFADDR"); ret = 1; }
            } else {
                fprintf(stderr, "ifconfig: unknown option '%s'\n", argv[i]);
                ret = 1;
            }
        }
    }

    close(fd);
    return ret;
}
