#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <sys/select.h>
#include <time.h>
#include "new_encryptor.h"

#define BASE_PATH "/mnt/mta/"
#define ENCRYPTER_PIPE BASE_PATH "encrypter_pipe"
#define CONFIG_FILE BASE_PATH "conf.txt"
#define LOG_FILE "/var/log/encrypter.log"
#define MAX_DECRYPTERS 100

struct decrypter_info decrypters[MAX_DECRYPTERS];
int num_decrypters = 0;

char *current_cipher = NULL;
size_t cipher_len = 0;

void log_message(const char *msg) {
    FILE *logf = fopen(LOG_FILE, "a");
    if (logf) {
        fprintf(logf, "%s\n", msg);
        fclose(logf);
    }
}

void broadcast_cipher() {
    for (int i = 0; i < num_decrypters; ++i) {
        int fd = open(decrypters[i].in_pipe, O_WRONLY);
        if (fd < 0) continue;
        write(fd, &cipher_len, sizeof(cipher_len));
        write(fd, current_cipher, cipher_len);
        close(fd);
    }
}

int main() {
    srand(time(NULL));

    // Read password length from config
    FILE *conf = fopen(CONFIG_FILE, "r");
    if (!conf) { perror("config"); exit(1); }
    fscanf(conf, "%zu", &cipher_len);
    fclose(conf);

    // Generate initial cipher (random bytes)
    current_cipher = malloc(cipher_len);
    for (size_t i = 0; i < cipher_len; i++)
        // TODO change to use MTA rand
        current_cipher[i] = 'A' + (rand() % 26);

    // Create subscription pipe
    unlink(ENCRYPTER_PIPE);
    if (mkfifo(ENCRYPTER_PIPE, 0666) < 0 && errno != EEXIST) {
        perror("mkfifo");
        exit(1);
    }
    int sub_fd = open(ENCRYPTER_PIPE, O_RDONLY | O_NONBLOCK);
    if (sub_fd < 0) { perror("open encrypter_pipe"); exit(1); }

    log_message("[ENCRYPTER] Started");

    // Main loop
    while (1) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(sub_fd, &readfds);
        int maxfd = sub_fd;

        // Monitor all out_pipes for guesses
        int out_fds[MAX_DECRYPTERS];
        for (int i = 0; i < num_decrypters; i++) {
            out_fds[i] = open(decrypters[i].out_pipe, O_RDONLY | O_NONBLOCK);
            if (out_fds[i] >= 0) {
                FD_SET(out_fds[i], &readfds);
                if (out_fds[i] > maxfd) maxfd = out_fds[i];
            }
        }

        // Wait for any event
        int ready = select(maxfd + 1, &readfds, NULL, NULL, NULL);
        if (ready < 0) { perror("select"); exit(1); }

        // Handle subscription
        if (FD_ISSET(sub_fd, &readfds)) {
            char buf[512];
            int n = read(sub_fd, buf, sizeof(buf) - 1);
            if (n > 0) {
                buf[n] = '\0';
                char in_pipe[256], out_pipe[256];
                sscanf(buf, "%s %s", in_pipe, out_pipe);

                if (num_decrypters < MAX_DECRYPTERS) {
                    strcpy(decrypters[num_decrypters].in_pipe, in_pipe);
                    strcpy(decrypters[num_decrypters].out_pipe, out_pipe);
                    num_decrypters++;

                    // Send current cipher to new subscriber
                    int fd = open(in_pipe, O_WRONLY);
                    if (fd >= 0) {
                        write(fd, &cipher_len, sizeof(cipher_len));
                        write(fd, current_cipher, cipher_len);
                        close(fd);
                    }

                    char logbuf[512];
                    snprintf(logbuf, sizeof(logbuf), "[ENCRYPTER] New subscription: %s / %s", in_pipe, out_pipe);
                    log_message(logbuf);
                }
            }
        }

        // Handle guesses
        for (int i = 0; i < num_decrypters; i++) {
            if (out_fds[i] >= 0 && FD_ISSET(out_fds[i], &readfds)) {
                unsigned int guess_len;
                char guess[512];
                read(out_fds[i], &guess_len, sizeof(guess_len));
                read(out_fds[i], guess, guess_len);
                guess[guess_len] = '\0';

                char logbuf[512];
                snprintf(logbuf, sizeof(logbuf), "[ENCRYPTER] Received guess from %s: %s",
                         decrypters[i].out_pipe, guess);
                log_message(logbuf);

                // Validate (for demo: accept any guess)
                // In real code, validate guess here
                printf("[ENCRYPTER] Guess accepted! Broadcasting new password...\n");

                // Generate and broadcast new password
                for (size_t j = 0; j < cipher_len; j++)
                    current_cipher[j] = 'A' + (rand() % 26);
                broadcast_cipher();
            }
            if (out_fds[i] >= 0) close(out_fds[i]);
        }
    }

    return 0;
}
