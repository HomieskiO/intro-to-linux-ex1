#ifndef DECRYPTER_H
#define DECRYPTER_H

#include <mta_crypt.h>
#include <mta_rand.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>

// ---------- CONSTANTS ----------
#define BASE_PATH           "/mnt/mta/"
#define ENCRYPTER_PIPE      BASE_PATH "encrypter_pipe"
#define DECRYPTER_IN_FMT    BASE_PATH "decrypter_in_%d"
#define DECRYPTER_OUT_FMT   BASE_PATH "decrypter_out_%d"
#define MAX_DECRYPTERS      100

// ---------- FUNCTION DECLARATIONS ----------

/**
 * @brief Find the next available decrypter ID (1..MAX_DECRYPTERS)
 * by checking for missing pipes.
 *
 * @return The available ID or -1 if none.
 */
int find_next_vacant_id(void);

/**
 * @brief Try to read a ciphertext message from the input pipe (non-blocking).
 *
 * @param fd  Input pipe file descriptor.
 * @param buf Pointer to allocated buffer with ciphertext (must be freed by caller).
 * @param len Pointer to ciphertext length.
 * @return 0 if message was read, -1 if no data available.
 */
int try_read_cipher(int fd, char **buf, size_t *len);

/**
 * @brief Submit a plaintext guess back to the Encrypter through the output pipe.
 *
 * @param out_pipe   Name of the output pipe.
 * @param decrypted  Pointer to the guessed plaintext.
 * @param key        Pointer to the key used.
 * @param cipher_len Ciphertext length.
 * @param key_len    Key length.
 * @param iterations Number of iterations attempted.
 */
void submit_guess(const char *out_pipe, const char *decrypted, const char *key,
                  size_t cipher_len, size_t key_len, long iterations);

/**
 * @brief Brute-force the ciphertext until a guess is found and sent.
 *
 * @param ciphertext Pointer to ciphertext buffer.
 * @param cipher_len Length of ciphertext.
 * @param out_pipe   Output pipe name for guesses.
 */
void brute_force(const char *ciphertext, size_t cipher_len, const char *out_pipe);

#endif // DECRYPTER_H
