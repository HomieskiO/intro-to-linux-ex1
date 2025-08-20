#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include "new_decryptor.h"


// Utility: find next vacant decrypter ID
int find_next_vacant_id() {
    char path[256];
    for (int i = 1; i <= MAX_DECRYPTERS; ++i) {
        snprintf(path, sizeof(path), DECRYPTER_IN_FMT, i);
        if (access(path, F_OK) != 0) {
            return i;
        }
    }
    return -1;
}

int try_read_cipher(int fd, char **buf, size_t *len) {
    unsigned int clen;
    ssize_t n = read(fd, &clen, sizeof(clen));
    if (n == -1 && (errno == EAGAIN || errno == EWOULDBLOCK))
        return -1;
    if (n == 0) return -1; // no data
    if (n != sizeof(clen)) { perror("read len"); exit(1); }

    *len = clen;
    *buf = malloc(*len);
    if (!*buf) { perror("malloc cipher"); exit(1); }
    n = read(fd, *buf, *len);
    if (n != (ssize_t)*len) { perror("read cipher data"); exit(1); }
    return 0;
}

static int is_all_printable(const char *text, size_t len) {
    for (size_t i = 0; i < len; i++)
        if (!isprint(text[i])) return 0;
    return 1;
}

// Send guessed plaintext back to encrypter
void submit_guess(const char *out_pipe, const char *decrypted, const char *key,
                  size_t cipher_len, size_t key_len, long iterations) {
    int fd = open(out_pipe, O_WRONLY);
    if (fd < 0) { perror("open out_pipe"); return; }

    unsigned int dlen = cipher_len;
    write(fd, &dlen, sizeof(dlen));
    write(fd, decrypted, dlen);
    write(fd, key, key_len);
    write(fd, &iterations, sizeof(iterations));
    close(fd);

    printf("[DECRYPTER] Sent guess after %ld iterations\n", iterations);
}

void brute_force(const char *ciphertext, size_t cipher_len, const char *out_pipe) {
    size_t key_len = cipher_len / 8; // assume cipher_len multiple of 8
    char *trial_key = malloc(key_len);
    char *decrypted = malloc(cipher_len);
    if (!trial_key || !decrypted) { perror("malloc"); exit(1); }

    long iterations = 0;

    while (1) {
        iterations++;
        MTA_get_rand_data(trial_key, key_len);
        unsigned int out_len = (unsigned int)cipher_len;

        if (MTA_decrypt(trial_key, (unsigned int)key_len, ciphertext,
                        (unsigned int)cipher_len, decrypted, &out_len) != MTA_CRYPT_RET_OK)
            continue;

        if (!is_all_printable(decrypted, cipher_len))
            continue;

        submit_guess(out_pipe, decrypted, trial_key, cipher_len, key_len, iterations);
        // Keep going if assignment requires; break for demo
        break;
    }

    free(trial_key);
    free(decrypted);
}

int main() {
    // FIXED: Initialize MTA crypto system
    MTA_crypt_init();

    // Find next available ID
    int id = find_next_vacant_id();
    if (id < 0) { fprintf(stderr, "No available decrypter IDs\n"); exit(1); }

    // Build pipe names
    char in_pipe[256], out_pipe[256];
    snprintf(in_pipe, sizeof(in_pipe), DECRYPTER_IN_FMT, id);
    snprintf(out_pipe, sizeof(out_pipe), DECRYPTER_OUT_FMT, id);

    // Create pipes
    mkfifo(in_pipe, 0666);
    mkfifo(out_pipe, 0666);

    // Subscribe to encrypter with both pipes
    int sub_fd = open(ENCRYPTER_PIPE, O_WRONLY);
    if (sub_fd < 0) { perror("open encrypter_pipe"); exit(1); }
    dprintf(sub_fd, "%s %s\n", in_pipe, out_pipe);
    close(sub_fd);

    printf("[DECRYPTER] Subscribed with pipes: %s / %s\n", in_pipe, out_pipe);

    // Open in_pipe for reading (non-blocking)
    int work_fd = open(in_pipe, O_RDONLY | O_NONBLOCK);
    if (work_fd < 0) { perror("open in_pipe"); exit(1); }

    // Wait for ciphertexts and brute-force
    while (1) {
        char *ciphertext = NULL;
        size_t cipher_len = 0;
        if (try_read_cipher(work_fd, &ciphertext, &cipher_len) == 0) {
            printf("[DECRYPTER] Received cipher (%zu bytes)\n", cipher_len);
            brute_force(ciphertext, cipher_len, out_pipe);
            free(ciphertext);
        }
        usleep(50000); // avoid busy loop
    }

    close(work_fd);
    unlink(in_pipe);
    unlink(out_pipe);
    return 0;
}
