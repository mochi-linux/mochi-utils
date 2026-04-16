#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/* Linux /proc/net/tcp state numbers (1-based) */
static const char *tcp_states[] = {
    "",
    "ESTABLISHED", "SYN_SENT",  "SYN_RECV",
    "FIN_WAIT1",   "FIN_WAIT2", "TIME_WAIT",
    "CLOSE",       "CLOSE_WAIT","LAST_ACK",
    "LISTEN",      "CLOSING"
};

/* Parse a "XXXXXXXX:PPPP" hex token from /proc/net/tcp|udp into ip_str and port.
   Linux stores the address in host-byte-order on little-endian systems. */
static void parse_hex_endpoint(const char *tok,
                                char *ip_str, size_t ip_sz,
                                int *port) {
    unsigned int ip, p;
    if (sscanf(tok, "%8X:%4X", &ip, &p) != 2) {
        snprintf(ip_str, ip_sz, "?");
        *port = 0;
        return;
    }
    /* Extract bytes in network order (reverse of little-endian uint32) */
    unsigned int a = (ip >>  0) & 0xFF;
    unsigned int b = (ip >>  8) & 0xFF;
    unsigned int c = (ip >> 16) & 0xFF;
    unsigned int d = (ip >> 24) & 0xFF;
    snprintf(ip_str, ip_sz, "%u.%u.%u.%u", a, b, c, d);
    *port = (int)p;
}

static void show_tcp(void) {
    FILE *f = fopen("/proc/net/tcp", "r");
    if (!f) return;

    printf("%-5s %-25s %-25s %s\n",
        "Proto", "Local Address", "Foreign Address", "State");

    char line[256];
    fgets(line, sizeof(line), f); /* skip header */

    while (fgets(line, sizeof(line), f)) {
        char local_tok[20], rem_tok[20];
        unsigned int state;

        if (sscanf(line, " %*d: %19s %19s %X",
                   local_tok, rem_tok, &state) != 3)
            continue;

        char local_ip[INET_ADDRSTRLEN], rem_ip[INET_ADDRSTRLEN];
        int  local_port, rem_port;
        parse_hex_endpoint(local_tok, local_ip, sizeof(local_ip), &local_port);
        parse_hex_endpoint(rem_tok,   rem_ip,   sizeof(rem_ip),   &rem_port);

        char lstr[32], rstr[32];
        snprintf(lstr, sizeof(lstr), "%s:%d", local_ip, local_port);
        snprintf(rstr, sizeof(rstr), "%s:%d", rem_ip,   rem_port);

        const char *sname = (state < 12) ? tcp_states[state] : "UNKNOWN";
        printf("%-5s %-25s %-25s %s\n", "tcp", lstr, rstr, sname);
    }
    fclose(f);
}

static void show_udp(void) {
    FILE *f = fopen("/proc/net/udp", "r");
    if (!f) return;

    char line[256];
    fgets(line, sizeof(line), f);

    while (fgets(line, sizeof(line), f)) {
        char local_tok[20], rem_tok[20];
        unsigned int state;

        if (sscanf(line, " %*d: %19s %19s %X",
                   local_tok, rem_tok, &state) != 3)
            continue;

        char local_ip[INET_ADDRSTRLEN], rem_ip[INET_ADDRSTRLEN];
        int  local_port, rem_port;
        parse_hex_endpoint(local_tok, local_ip, sizeof(local_ip), &local_port);
        parse_hex_endpoint(rem_tok,   rem_ip,   sizeof(rem_ip),   &rem_port);

        char lstr[32], rstr[32];
        snprintf(lstr, sizeof(lstr), "%s:%d", local_ip, local_port);
        snprintf(rstr, sizeof(rstr), "%s:%d", rem_ip,   rem_port);

        printf("%-5s %-25s %-25s\n", "udp", lstr, rstr);
    }
    fclose(f);
}

static void show_interfaces(void) {
    FILE *f = fopen("/proc/net/dev", "r");
    if (!f) return;

    printf("\n%-12s %12s %10s %8s %12s %10s %8s\n",
        "Iface", "RX-bytes", "RX-pkts", "RX-errs",
        "TX-bytes", "TX-pkts", "TX-errs");

    char line[256];
    fgets(line, sizeof(line), f); /* header 1 */
    fgets(line, sizeof(line), f); /* header 2 */

    while (fgets(line, sizeof(line), f)) {
        char *colon = strchr(line, ':');
        if (!colon) continue;
        *colon = '\0';
        char *name = line;
        while (*name == ' ') name++;

        unsigned long rx_bytes, rx_pkts, rx_errs, rx_drop;
        unsigned long tx_bytes, tx_pkts, tx_errs, tx_drop;
        sscanf(colon + 1,
            "%lu %lu %lu %lu %*u %*u %*u %*u"
            " %lu %lu %lu %lu",
            &rx_bytes, &rx_pkts, &rx_errs, &rx_drop,
            &tx_bytes, &tx_pkts, &tx_errs, &tx_drop);

        printf("%-12s %12lu %10lu %8lu %12lu %10lu %8lu\n",
            name, rx_bytes, rx_pkts, rx_errs,
                  tx_bytes, tx_pkts, tx_errs);
    }
    fclose(f);
}

static void show_help(void) {
    printf("Usage: netstat [-a] [-u] [-i] [-t]\n");
    printf("  (no flags)  show TCP connections\n");
    printf("  -a          show all connections + interface stats\n");
    printf("  -u          also show UDP sockets\n");
    printf("  -i          show interface statistics only\n");
    printf("  -t          show TCP only (default)\n");
}

int netstat_main(int argc, char **argv) {
    int show_all  = 0;
    int show_iface = 0;
    int flag_udp   = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-a") == 0 || strcmp(argv[i], "--all") == 0)
            show_all = 1;
        else if (strcmp(argv[i], "-i") == 0 || strcmp(argv[i], "--interfaces") == 0)
            show_iface = 1;
        else if (strcmp(argv[i], "-u") == 0 || strcmp(argv[i], "--udp") == 0)
            flag_udp = 1;
        else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0)
            { show_help(); return 0; }
    }

    if (show_iface) {
        show_interfaces();
        return 0;
    }

    show_tcp();

    if (flag_udp || show_all)
        show_udp();

    if (show_all)
        show_interfaces();

    return 0;
}
