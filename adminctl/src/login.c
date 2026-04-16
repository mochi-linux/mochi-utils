#include "accman.h"
#include <stdio.h>
#include <pwd.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

int login_main(int argc, char **argv) {
    char *username;
    char user_buf[256];
    
    if (argc > 1) {
        username = argv[1];
    } else {
        printf("login: ");
        fflush(stdout);
        if (!fgets(user_buf, sizeof(user_buf), stdin)) return 1;
        user_buf[strcspn(user_buf, "\n")] = 0;
        username = user_buf;
    }

    char *pass = getpass("Password: ");
    if (!pass) return 1;

    if (accman_verify_password(username, pass) != 1) {
        fprintf(stderr, "Login incorrect\n");
        return 1;
    }

    struct passwd *pw = getpwnam(username);
    if (!pw) {
        fprintf(stderr, "Login incorrect\n");
        return 1;
    }

    if (setgid(pw->pw_gid) != 0) {
        perror("setgid");
        return 1;
    }
    
    if (setuid(pw->pw_uid) != 0) {
        perror("setuid");
        return 1;
    }

    if (chdir(pw->pw_dir) != 0) {
        perror("chdir");
    }
    setenv("HOME", pw->pw_dir, 1);
    setenv("USER", pw->pw_name, 1);
    
    char *shell = pw->pw_shell;
    if (!shell || strlen(shell) == 0) shell = "/bin/sh";
    
    char *shell_name = strrchr(shell, '/');
    shell_name = shell_name ? shell_name + 1 : shell;
    
    char login_shell[256];
    snprintf(login_shell, sizeof(login_shell), "-%s", shell_name);

    execl(shell, login_shell, NULL);
    perror("execl");
    return 1;
}