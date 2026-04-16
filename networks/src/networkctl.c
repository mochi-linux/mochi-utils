#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libgen.h>

int ifconfig_main(int argc, char **argv);
int route_main(int argc, char **argv);
int ping_main(int argc, char **argv);
int hostname_main(int argc, char **argv);
int netstat_main(int argc, char **argv);
int dns_main(int argc, char **argv);

static const struct {
    const char *name;
    int (*fn)(int, char **);
} commands[] = {
    { "ifconfig", ifconfig_main },
    { "route",    route_main    },
    { "ping",     ping_main     },
    { "hostname", hostname_main },
    { "netstat",  netstat_main  },
    { "dns",      dns_main      },
    { NULL, NULL }
};

int main(int argc, char **argv) {
    char *name = basename(argv[0]);

    for (int i = 0; commands[i].name; i++) {
        if (strcmp(name, commands[i].name) == 0)
            return commands[i].fn(argc, argv);
    }

    if (argc > 1) {
        for (int i = 0; commands[i].name; i++) {
            if (strcmp(argv[1], commands[i].name) == 0)
                return commands[i].fn(argc - 1, argv + 1);
        }
    }

    fprintf(stderr, "Usage: %s <command> [args]\n", name);
    fprintf(stderr, "Commands: ifconfig, route, ping, hostname, netstat, dns\n");
    return 1;
}
