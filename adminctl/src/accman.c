#include "accman.h"
#include "sha256.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>

int accman_init(void) {
    struct stat st = {0};

    /* Ensure AccountManager directory exists securely */
    if (stat(ACCT_DIR_PATH, &st) == -1) {
        if (mkdir(ACCT_DIR_PATH, 0700) != 0) {
            return -1;
        }
    }

    /* Ensure ACCT binary file exists securely */
    if (stat(ACCT_FILE_PATH, &st) == -1) {
        int fd = open(ACCT_FILE_PATH, O_CREAT | O_WRONLY, 0600);
        if (fd < 0) {
            return -1;
        }
        close(fd);
    }

    return 0;
}

static void hash_password(const char *password, uint8_t *hash_out) {
    SHA256_CTX ctx;
    sha256_init(&ctx);
    sha256_update(&ctx, (const uint8_t *)password, strlen(password));
    sha256_final(&ctx, hash_out);
}

int accman_set_password(const char *username, const char *password) {
    if (!username || !password || strlen(username) >= MAX_USERNAME_LEN) {
        return -1;
    }

    if (accman_init() != 0) {
        return -1;
    }

    AcctRecord new_record;
    memset(&new_record, 0, sizeof(AcctRecord));
    strncpy(new_record.username, username, MAX_USERNAME_LEN - 1);
    hash_password(password, new_record.password_hash);

    FILE *fp = fopen(ACCT_FILE_PATH, "r+b");
    if (!fp) {
        /* If it fails to open in r+b, try w+b to create it */
        fp = fopen(ACCT_FILE_PATH, "w+b");
        if (!fp) {
            return -1;
        }
    }

    AcctRecord current_record;
    int found = 0;

    /* Search for existing user to update */
    while (fread(&current_record, sizeof(AcctRecord), 1, fp) == 1) {
        if (strcmp(current_record.username, username) == 0) {
            /* Found user, seek back one record size and overwrite */
            fseek(fp, -(long)sizeof(AcctRecord), SEEK_CUR);
            fwrite(&new_record, sizeof(AcctRecord), 1, fp);
            found = 1;
            break;
        }
    }

    /* If user wasn't found, append them to the end of the file */
    if (!found) {
        fseek(fp, 0, SEEK_END);
        fwrite(&new_record, sizeof(AcctRecord), 1, fp);
    }

    fclose(fp);
    return 0;
}

int accman_verify_password(const char *username, const char *password) {
    if (!username || !password) {
        return -1;
    }

    FILE *fp = fopen(ACCT_FILE_PATH, "rb");
    if (!fp) {
        /* Cannot open file, system might be uninitialized or user not found */
        return -1; 
    }

    uint8_t target_hash[HASH_LEN];
    hash_password(password, target_hash);

    AcctRecord current_record;
    int status = -1; /* default to -1 (user not found) */

    while (fread(&current_record, sizeof(AcctRecord), 1, fp) == 1) {
        if (strcmp(current_record.username, username) == 0) {
            /* Verify the password hash */
            if (memcmp(current_record.password_hash, target_hash, HASH_LEN) == 0) {
                status = 1; /* Match */
            } else {
                status = 0; /* Invalid password */
            }
            break;
        }
    }

    fclose(fp);
    return status;
}