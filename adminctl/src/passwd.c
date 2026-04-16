#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pwd.h>
#include <errno.h>
#include "accman.h"

int passwd_main(int argc, char **argv) {
    char *username;
    if (argc < 2) {
        struct passwd *pw = getpwuid(getuid());
        if (!pw) {
            fprintf(stderr, "Cannot determine username.\n");
            return 1;
        }
        username = pw->pw_name;
    } else {
        username = argv[1];
    }

    /* Check if user exists */
    if (getpwnam(username) == NULL) {
        fprintf(stderr, "User '%s' not found.\n", username);
        return 1;
    }

    /* Non-root users must prove their current password before changing it */
    if (getuid() != 0) {
        char *current = getpass("Current password: ");
        if (!current) return 1;
        int result = accman_verify_password(username, current);
        if (result != 1) {
            fprintf(stderr, "Authentication failure.\n");
            return 1;
        }
    }

    char *pass = getpass("New password: ");
    if (!pass) return 1;

    /* Enforce a minimum password length */
    if (strlen(pass) < 6) {
        fprintf(stderr, "Password must be at least 6 characters.\n");
        return 1;
    }

    char *pass_confirm = getpass("Retype new password: ");
    if (!pass_confirm || strcmp(pass, pass_confirm) != 0) {
        fprintf(stderr, "Passwords do not match.\n");
        return 1;
    }

    if (accman_set_password(username, pass) != 0) {
        fprintf(stderr, "Failed to update password for user '%s'.\n", username);
        return 1;
    }

    printf("Password updated successfully for '%s'\n", username);
    return 0;
}
