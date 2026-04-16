#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libgen.h>

int adduser_main(int argc, char **argv);
int passwd_main(int argc, char **argv);
int login_main(int argc, char **argv);

int main(int argc, char **argv) {
    char *name = basename(argv[0]);

    if (strcmp(name, "adduser") == 0) {
        return adduser_main(argc, argv);
    } else if (strcmp(name, "passwd") == 0) {
        return passwd_main(argc, argv);
    } else if (strcmp(name, "login") == 0) {
        return login_main(argc, argv);
    } else if (argc > 1) {
        if (strcmp(argv[1], "adduser") == 0) {
            return adduser_main(argc - 1, argv + 1);
        } else if (strcmp(argv[1], "passwd") == 0) {
            return passwd_main(argc - 1, argv + 1);
        } else if (strcmp(argv[1], "login") == 0) {
            return login_main(argc - 1, argv + 1);
        }
    }

    fprintf(stderr, "Usage: %s <command> [args]\n", name);
    fprintf(stderr, "Available commands: adduser, passwd, login\n");
    return 1;
}
