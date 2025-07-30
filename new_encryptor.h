#ifndef ENCRYPTOR_H
#define ENCRYPTOR_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <sys/select.h>
#include <time.h>

// ---------- CONSTANTS ----------
#define BASE_PATH "/mnt/mta/"
#define ENCRYPTER_PIPE BASE_PATH "encrypter_pipe"
#define CONFIG_FILE BASE_PATH "conf.txt"
#define LOG_FILE "/var/log/encrypter.log"
#define MAX_DECRYPTERS 100

// ---------- STRUCTURES ----------
struct decrypter_info {
    char in_pipe[256];   // Pipe name for sending cipher (encryptor -> decrypter)
    char out_pipe[256];  // Pipe name for receiving guesses (decrypter -> encryptor)
};

// ---------- GLOBALS ----------
extern struct decrypter_info decrypters[MAX_DECRYPTERS];
extern int num_decrypters;

extern char *current_cipher;
extern size_t cipher_len;

// ---------- FUNCTIONS ----------
/**
 * @brief Logs a message to the log file.
 *
 * @param msg Message to log.
 */
void log_message(const char *msg);

/**
 * @brief Broadcasts the current cipher to all connected decrypters.
 */
void broadcast_cipher(void);

#endif // ENCRYPTOR_H
