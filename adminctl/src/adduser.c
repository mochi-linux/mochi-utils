#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pwd.h>
#include <grp.h>
#include "accman.h"

static int validate_username(const char *username) {
    if (!username || strlen(username) == 0 || strlen(username) >= MAX_USERNAME_LEN)
        return 0;
    /* Must start with a letter */
    if (!isalpha((unsigned char)username[0]))
        return 0;
    /* Allow alphanumeric, underscore, hyphen */
    for (const char *p = username + 1; *p; p++) {
        if (!isalnum((unsigned char)*p) && *p != '_' && *p != '-')
            return 0;
    }
    return 1;
}

int adduser_main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: adduser <username>\n");
        return 1;
    }

    char *username = argv[1];

    /* Validate username format */
    if (!validate_username(username)) {
        fprintf(stderr, "Invalid username '%s'. Must start with a letter and contain only "
                "letters, digits, '-', or '_'.\n", username);
        return 1;
    }

    /* Check if user already exists in /etc/passwd */
    if (getpwnam(username) != NULL) {
        fprintf(stderr, "User '%s' already exists.\n", username);
        return 1;
    }

    // Find next available UID
    uid_t next_uid = 1000;
    struct passwd *p;
    setpwent();
    while ((p = getpwent()) != NULL) {
        if (p->pw_uid >= next_uid) {
            next_uid = p->pw_uid + 1;
        }
    }
    endpwent();

    // Append to /etc/passwd
    FILE *fp = fopen("/etc/passwd", "a");
    if (!fp) {
        perror("fopen /etc/passwd");
        return 1;
    }

    fprintf(fp, "%s:x:%u:%u:%s:/Users/%s:/bin/bash\n", 
            username, next_uid, next_uid, username, username);
    fclose(fp);

    // Append to /etc/group
    fp = fopen("/etc/group", "a");
    if (fp) {
        fprintf(fp, "%s:x:%u:\n", username, next_uid);
        fclose(fp);
    }

    // Create home directory
    char home_dir[256];
    snprintf(home_dir, sizeof(home_dir), "/Users/%s", username);
    if (mkdir(home_dir, 0755) != 0) {
        perror("mkdir home directory");
    } else {
        chown(home_dir, next_uid, next_uid);
    }

    printf("User '%s' added with UID %u\n", username, next_uid);

    /* Prompt for an initial password */
    char *pass = getpass("New password: ");
    if (pass && strlen(pass) > 0) {
        char *confirm = getpass("Retype new password: ");
        if (confirm && strcmp(pass, confirm) == 0) {
            if (accman_set_password(username, pass) == 0) {
                printf("Password set for '%s'.\n", username);
            } else {
                fprintf(stderr, "Warning: failed to set password for '%s'.\n", username);
            }
        } else {
            fprintf(stderr, "Warning: passwords do not match. No password set.\n");
        }
    } else {
        fprintf(stderr, "Warning: empty password given. Account will have no password.\n");
    }

    return 0;
}
