#include "accman.h"
#include "sha256.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

int accman_init(void) {
    struct stat st;

    /* Ensure all parent directories exist with secure permissions */
    const char *dirs[] = {
        "/System",
        "/System/Library",
        "/System/Library/Security",
        ACCT_DIR_PATH,
        NULL
    };

    for (int i = 0; dirs[i] != NULL; i++) {
        if (stat(dirs[i], &st) == -1) {
            if (mkdir(dirs[i], 0700) != 0 && errno != EEXIST) {
                return -1;
            }
        }
    }

    /* Ensure ACCT binary file exists with restricted permissions */
    if (stat(ACCT_FILE_PATH, &st) == -1) {
        int fd = open(ACCT_FILE_PATH, O_CREAT | O_WRONLY | O_EXCL, 0600);
        if (fd < 0 && errno != EEXIST) {
            return -1;
        }
        if (fd >= 0) close(fd);
    }

    return 0;
}

static int generate_salt(uint8_t *salt) {
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd < 0) return -1;
    ssize_t n = read(fd, salt, SALT_LEN);
    close(fd);
    return (n == (ssize_t)SALT_LEN) ? 0 : -1;
}

static void hash_password_salted(const char *password, const uint8_t *salt, uint8_t *hash_out) {
    SHA256_CTX ctx;
    sha256_init(&ctx);
    sha256_update(&ctx, salt, SALT_LEN);
    sha256_update(&ctx, (const uint8_t *)password, strlen(password));
    sha256_final(&ctx, hash_out);
}

int accman_set_password(const char *username, const char *password) {
    if (!username || !password ||
        strlen(username) == 0 || strlen(username) >= MAX_USERNAME_LEN) {
        return -1;
    }

    if (accman_init() != 0) {
        return -1;
    }

    /* Generate a fresh random salt for this password */
    uint8_t salt[SALT_LEN];
    if (generate_salt(salt) != 0) {
        /* Fallback: derive a deterministic salt from the username */
        memset(salt, 0, SALT_LEN);
        memcpy(salt, username, strlen(username) < SALT_LEN ? strlen(username) : SALT_LEN);
    }

    AcctRecord new_record;
    memset(&new_record, 0, sizeof(AcctRecord));
    strncpy(new_record.username, username, MAX_USERNAME_LEN - 1);
    memcpy(new_record.salt, salt, SALT_LEN);
    hash_password_salted(password, salt, new_record.password_hash);

    /* Open for read/write; only fall back to create when the file is absent */
    FILE *fp = fopen(ACCT_FILE_PATH, "r+b");
    if (!fp) {
        if (errno != ENOENT) {
            return -1;
        }
        fp = fopen(ACCT_FILE_PATH, "w+b");
        if (!fp) {
            return -1;
        }
    }

    AcctRecord current_record;
    int found = 0;

    /* Search for an existing record to update in place */
    while (fread(&current_record, sizeof(AcctRecord), 1, fp) == 1) {
        if (strcmp(current_record.username, username) == 0) {
            fseek(fp, -(long)sizeof(AcctRecord), SEEK_CUR);
            if (fwrite(&new_record, sizeof(AcctRecord), 1, fp) != 1) {
                fclose(fp);
                return -1;
            }
            found = 1;
            break;
        }
    }

    /* User not found: append a new record */
    if (!found) {
        fseek(fp, 0, SEEK_END);
        if (fwrite(&new_record, sizeof(AcctRecord), 1, fp) != 1) {
            fclose(fp);
            return -1;
        }
    }

    fclose(fp);
    /* Wipe sensitive data from the stack */
    memset(&new_record, 0, sizeof(AcctRecord));
    memset(salt, 0, SALT_LEN);
    return 0;
}

int accman_verify_password(const char *username, const char *password) {
    if (!username || !password) {
        return -1;
    }

    FILE *fp = fopen(ACCT_FILE_PATH, "rb");
    if (!fp) {
        return -1;
    }

    AcctRecord current_record;
    int status = -1; /* default: user not found */

    while (fread(&current_record, sizeof(AcctRecord), 1, fp) == 1) {
        if (strcmp(current_record.username, username) == 0) {
            uint8_t target_hash[HASH_LEN];
            hash_password_salted(password, current_record.salt, target_hash);
            if (memcmp(current_record.password_hash, target_hash, HASH_LEN) == 0) {
                status = 1; /* Match */
            } else {
                status = 0; /* Wrong password */
            }
            /* Wipe hash from stack before returning */
            memset(target_hash, 0, HASH_LEN);
            break;
        }
    }

    fclose(fp);
    return status;
}

int accman_delete_user(const char *username) {
    if (!username || strlen(username) == 0) {
        return -1;
    }

    FILE *fp = fopen(ACCT_FILE_PATH, "rb");
    if (!fp) {
        return -1;
    }

    /* Load every record except the target user */
    AcctRecord records[1024];
    int count = 0;
    int found = 0;
    AcctRecord rec;

    while (fread(&rec, sizeof(AcctRecord), 1, fp) == 1) {
        if (strcmp(rec.username, username) == 0) {
            found = 1;
        } else {
            if (count < (int)(sizeof(records) / sizeof(records[0]))) {
                records[count++] = rec;
            }
        }
    }
    fclose(fp);

    if (!found) {
        return -1;
    }

    /* Rewrite the file without the deleted user */
    fp = fopen(ACCT_FILE_PATH, "wb");
    if (!fp) {
        return -1;
    }

    for (int i = 0; i < count; i++) {
        fwrite(&records[i], sizeof(AcctRecord), 1, fp);
    }
    fclose(fp);
    return 0;
}

int accman_user_exists(const char *username) {
    if (!username || strlen(username) == 0) {
        return 0;
    }

    FILE *fp = fopen(ACCT_FILE_PATH, "rb");
    if (!fp) {
        return 0;
    }

    AcctRecord rec;
    int found = 0;
    while (fread(&rec, sizeof(AcctRecord), 1, fp) == 1) {
        if (strcmp(rec.username, username) == 0) {
            found = 1;
            break;
        }
    }
    fclose(fp);
    return found;
}