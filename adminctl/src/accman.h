#ifndef ACCMAN_H
#define ACCMAN_H

#include <stdint.h>
#include <stddef.h>

/* Define the storage paths for the encrypted/hashed binary Account store */
#define ACCT_DIR_PATH "/System/Library/Security/AccountManager"
#define ACCT_FILE_PATH "/System/Library/Security/AccountManager/ACCT"

#define MAX_USERNAME_LEN 32
#define HASH_LEN 32  /* SHA-256 produces a 32-byte hash */
#define SALT_LEN 16  /* 128-bit random salt per user record */

/* Structure representing a single user record in the ACCT binary file */
typedef struct {
    char    username[MAX_USERNAME_LEN];
    uint8_t salt[SALT_LEN];
    uint8_t password_hash[HASH_LEN];
} AcctRecord;

/**
 * Initializes the AccountManager directory and ACCT file if they don't exist.
 * Returns 0 on success, -1 on failure.
 */
int accman_init(void);

/**
 * Sets or updates the password for a given user.
 * Hashes the plaintext password using SHA-256 and stores it in the ACCT binary file.
 * Returns 0 on success, -1 on failure.
 */
int accman_set_password(const char *username, const char *password);

/**
 * Verifies the password for a given user.
 * Hashes the provided password and compares it against the stored hash in ACCT.
 * Returns 1 if valid, 0 if invalid, -1 on error (e.g., user not found).
 */
int accman_verify_password(const char *username, const char *password);

/**
 * Deletes the account record for the given user.
 * Rewrites the ACCT file omitting that record.
 * Returns 0 on success, -1 if the user was not found or on I/O error.
 */
int accman_delete_user(const char *username);

/**
 * Checks whether a user account exists in the ACCT store.
 * Returns 1 if found, 0 if not found.
 */
int accman_user_exists(const char *username);

#endif /* ACCMAN_H */