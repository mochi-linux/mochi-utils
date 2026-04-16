#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pwd.h>
#include <grp.h>

int adduser_main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: adduser <username>\n");
        return 1;
    }

    char *username = argv[1];
    
    // Check if user already exists
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
    return 0;
}
