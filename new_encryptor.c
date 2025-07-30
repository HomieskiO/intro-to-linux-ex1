#define _POSIX_C_SOURCE 200809L

#include <mta_crypt.h>
#include <mta_rand.h>

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <sys/select.h>
#include <time.h>
#include <stdint.h>

#define BASE_PATH           "/mnt/mta/"
#define ENCRYPTER_PIPE      BASE_PATH "encrypter_pipe"
#define CONFIG_FILE         BASE_PATH "conf.txt"
#define LOG_FILE            "/var/log/encrypter.log"
#define MAX_DECRYPTERS      100
#define MAX_PIPE_NAME       256
#define PIPEBUF             4096  /* typical PIPE_BUF; small messages stay atomic */

/* ---------------------------- Data structures ---------------------------- */

struct decrypter_info {
    char in_pipe[MAX_PIPE_NAME];   /* Encryptor -> Decrypter (ciphertext) */
    char out_pipe[MAX_PIPE_NAME];  /* Decrypter -> Encryptor (guesses)    */
    int  out_fd;                   /* opened O_RDONLY|O_NONBLOCK (if openable) */
};

static struct decrypter_info g_decs[MAX_DECRYPTERS];
static int g_num_decrypters = 0;

/* Current cycle state */
static char   *g_plaintext = NULL;   /* printable password (length = g_pass_len) */
static size_t  g_pass_len = 0;       /* read from CONFIG_FILE; must be multiple of 8 */
static char   *g_key = NULL;         /* random key (length = g_key_len) */
static size_t  g_key_len = 0;
static char   *g_cipher = NULL;      /* encrypted bytes (length = g_pass_len) */
static size_t  g_cipher_len = 0;

/* Subscription pipe fds */
static int g_sub_rd = -1;    /* /mnt/mta/encrypter_pipe (O_RDONLY|O_NONBLOCK) */
static int g_sub_wr_hold = -1; /* dummy O_WRONLY to avoid EOF when no writers */

/* ------------------------------- Utilities ------------------------------- */

static void log_message(const char *msg)
{
    FILE *f = fopen(LOG_FILE, "a");
    if (!f) return;
    time_t t = time(NULL);
    struct tm tmv;
    localtime_r(&t, &tmv);
    char ts[32];
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &tmv);
    fprintf(f, "[%s] %s\n", ts, msg);
    fclose(f);
}

static void die(const char *what)
{
    perror(what);
    exit(EXIT_FAILURE);
}

static void *xmalloc(size_t n)
{
    void *p = malloc(n);
    if (!p) {
        fprintf(stderr, "malloc(%zu) failed\n", n);
        exit(EXIT_FAILURE);
    }
    return p;
}

/* Read exactly n bytes (blocking with select-prepared readiness), handling EAGAIN.
   Returns 0 on success, -1 on clean EOF before full read, dies on hard error. */
static int read_exact(int fd, void *buf, size_t n)
{
    size_t got = 0;
    while (got < n) {
        ssize_t r = read(fd, (char*)buf + got, n - got);
        if (r > 0) {
            got += (size_t)r;
        } else if (r == 0) {
            /* writer closed end or no writer currently connected */
            return -1;
        } else {
            if (errno == EINTR) continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                /* caller should have selected; treat as shortfall */
                usleep(1000);
                continue;
            }
            die("read");
        }
    }
    return 0;
}

/* Write exactly n bytes; die on error. */
static void write_exact(int fd, const void *buf, size_t n)
{
    size_t sent = 0;
    while (sent < n) {
        ssize_t w = write(fd, (const char*)buf + sent, n - sent);
        if (w > 0) {
            sent += (size_t)w;
        } else if (w == 0) {
            /* shouldn't happen for FIFO with writer */
            die("write returned 0");
        } else {
            if (errno == EINTR) continue;
            die("write");
        }
    }
}

/* -------------------- MTA-based generation & encryption ------------------ */

static char mta_get_printable_char(void)
{
    /* Use MTA_get_rand_char until printable */
    for (;;) {
        char c = MTA_get_rand_char();
        if (isprint((unsigned char)c))
            return c;
    }
}

static void generate_printable_password(size_t len)
{
    if (g_plaintext) { free(g_plaintext); g_plaintext = NULL; }
    g_plaintext = (char*)xmalloc(len);
    for (size_t i = 0; i < len; ++i) {
        g_plaintext[i] = mta_get_printable_char();
    }
}

static void generate_random_key(size_t len)
{
    if (g_key) { free(g_key); g_key = NULL; }
    g_key = (char*)xmalloc(len);
    MTA_get_rand_data(g_key, len);
}

static void encrypt_current_password(void)
{
    if (g_cipher) { free(g_cipher); g_cipher = NULL; }
    g_cipher = (char*)xmalloc(g_pass_len);

    unsigned int out_len = (unsigned int)g_pass_len;
    int st = MTA_encrypt(g_key, (unsigned int)g_key_len,
                         g_plaintext, (unsigned int)g_pass_len,
                         g_cipher, &out_len);
    if (st != MTA_CRYPT_RET_OK) {
        fprintf(stderr, "MTA_encrypt error: %d\n", st);
        exit(EXIT_FAILURE);
    }
    g_cipher_len = (size_t)out_len;
}

/* Generates a fresh printable plaintext, random key, and ciphertext */
static void generate_new_cycle(void)
{
    /* lengths */
    g_key_len = g_pass_len / 8;
    generate_printable_password(g_pass_len);
    generate_random_key(g_key_len);
    encrypt_current_password();

    char logbuf[256];
    snprintf(logbuf, sizeof(logbuf),
             "[ENCRYPTER] New cycle: pass_len=%zu, key_len=%zu, broadcasting ciphertext.",
             g_pass_len, g_key_len);
    log_message(logbuf);
}

/* ----------------------- Broadcast / subscription ------------------------ */

static void send_cipher_to_inpipe(const char *in_pipe)
{
    int fd = open(in_pipe, O_WRONLY);
    if (fd < 0) {
        /* decrypter might not be ready momentarily */
        return;
    }
    uint32_t clen = (uint32_t)g_cipher_len;
    write_exact(fd, &clen, sizeof(clen));
    write_exact(fd, g_cipher, g_cipher_len);
    close(fd);
}

static void broadcast_cipher(void)
{
    for (int i = 0; i < g_num_decrypters; ++i) {
        send_cipher_to_inpipe(g_decs[i].in_pipe);
    }
}

/* Adds a decrypter (in/out pipes), opens its out pipe for reading (non-blocking) */
static void add_decrypter(const char *in_pipe, const char *out_pipe)
{
    if (g_num_decrypters >= MAX_DECRYPTERS) return;
    strncpy(g_decs[g_num_decrypters].in_pipe,  in_pipe,  MAX_PIPE_NAME-1);
    g_decs[g_num_decrypters].in_pipe[MAX_PIPE_NAME-1] = '\0';
    strncpy(g_decs[g_num_decrypters].out_pipe, out_pipe, MAX_PIPE_NAME-1);
    g_decs[g_num_decrypters].out_pipe[MAX_PIPE_NAME-1] = '\0';

    /* Open OUT pipe for reading (non-blocking). If no writer yet, this still succeeds,
       and read() will return 0 until a writer connects. */
    int fd = open(out_pipe, O_RDONLY | O_NONBLOCK);
    if (fd < 0) {
        /* If it fails (rare), keep -1 and retry opening later */
        g_decs[g_num_decrypters].out_fd = -1;
    } else {
        g_decs[g_num_decrypters].out_fd = fd;
    }

    ++g_num_decrypters;

    /* Immediately send current ciphertext to this new decrypter */
    send_cipher_to_inpipe(in_pipe);

    char logbuf[512];
    snprintf(logbuf, sizeof(logbuf),
             "[ENCRYPTER] New decrypter subscribed: in=%s out=%s",
             in_pipe, out_pipe);
    log_message(logbuf);
}

/* Reads subscription lines from g_sub_rd; each line contains:
   "<in_pipe> <out_pipe>\n" */
static void handle_subscription_readable(void)
{
    char buf[PIPEBUF];
    ssize_t n = read(g_sub_rd, buf, sizeof(buf)-1);
    if (n <= 0) {
        /* 0 => no writers right now; -1 with EAGAIN => nothing to read */
        return;
    }
    buf[n] = '\0';

    /* Process possibly multiple lines; each is atomic under PIPE_BUF */
    char *saveptr = NULL;
    char *line = strtok_r(buf, "\n", &saveptr);
    while (line) {
        char in_pipe[MAX_PIPE_NAME], out_pipe[MAX_PIPE_NAME];
        in_pipe[0] = out_pipe[0] = '\0';

        /* Expect two tokens */
        if (sscanf(line, "%255s %255s", in_pipe, out_pipe) == 2) {
            add_decrypter(in_pipe, out_pipe);
        }
        line = strtok_r(NULL, "\n", &saveptr);
    }
}

/* Ensures out_fd is open; if previously failed, retry opening. */
static void ensure_out_fd_opened(struct decrypter_info *d)
{
    if (d->out_fd >= 0) return;
    int fd = open(d->out_pipe, O_RDONLY | O_NONBLOCK);
    if (fd >= 0) d->out_fd = fd;
}

/* ----------------------- Guess reception & validation -------------------- */

/* Returns 1 if guess is accepted (plaintext AND key correct), 0 otherwise. */
static int validate_guess(const char *guess_plain, size_t guess_len,
                          const char *guess_key, size_t guess_key_len)
{
    if (guess_len != g_pass_len) return 0;
    if (guess_key_len != g_key_len) return 0;

    /* Compare plaintext directly */
    if (memcmp(guess_plain, g_plaintext, g_pass_len) != 0) {
        return 0;
    }

    /* Re-encrypt with provided key and compare to current ciphertext */
    char *probe = (char*)xmalloc(g_pass_len);
    unsigned int out_len = (unsigned int)g_pass_len;
    int st = MTA_encrypt(guess_key, (unsigned int)g_key_len,
                         g_plaintext, (unsigned int)g_pass_len,
                         probe, &out_len);
    if (st != MTA_CRYPT_RET_OK) {
        free(probe);
        return 0;
    }

    int ok = (out_len == g_cipher_len) && (memcmp(probe, g_cipher, g_cipher_len) == 0);
    free(probe);
    return ok;
}

/* Reads one complete guess message from fd (if available).
   Format (as sent by your decrypter):
     [uint32_t len][plaintext bytes][key bytes (len/8)][long iterations]
   Returns:
     1 on success (and performs validation & potential broadcast),
     0 if not enough data / EOF at start,
    -1 if permanent error on this fd (we'll close and let it reopen). */
static int handle_one_guess_from_fd(int fd, const char *who_pipe)
{
    /* Read plaintext length */
    uint32_t ulen = 0;
    if (read_exact(fd, &ulen, sizeof(ulen)) != 0) {
        return 0; /* no full message ready; not fatal */
    }
    size_t guess_len = (size_t)ulen;

    /* Sanity guard */
    if (guess_len == 0 || guess_len > (1u<<20)) {
        /* Too big / invalid */
        log_message("[ENCRYPTER] Invalid guess length; ignoring.");
        return -1;
    }

    /* Read plaintext */
    char *guess_plain = (char*)xmalloc(guess_len);
    if (read_exact(fd, guess_plain, guess_len) != 0) {
        free(guess_plain);
        return -1;
    }

    /* Read key (length = guess_len/8) */
    size_t guess_key_len = guess_len / 8;
    char *guess_key = (char*)xmalloc(guess_key_len);
    if (read_exact(fd, guess_key, guess_key_len) != 0) {
        free(guess_plain);
        free(guess_key);
        return -1;
    }

    /* Read iterations (same size as sender's 'long') */
    long iterations = 0;
    if (read_exact(fd, &iterations, sizeof(iterations)) != 0) {
        free(guess_plain);
        free(guess_key);
        return -1;
    }

    /* Validate */
    int ok = validate_guess(guess_plain, guess_len, guess_key, guess_key_len);

    char logbuf[512];
    snprintf(logbuf, sizeof(logbuf),
             "[ENCRYPTER] Guess from %s: len=%zu key_len=%zu iter=%ld => %s",
             who_pipe, guess_len, guess_key_len, iterations, ok ? "ACCEPT" : "REJECT");
    log_message(logbuf);

    free(guess_plain);
    free(guess_key);

    if (ok) {
        /* Generate next cycle and broadcast */
        generate_new_cycle();
        broadcast_cipher();
    }

    return 1;
}

/* Handle all readable out_fds (one per decrypter). */
static void handle_guesses(fd_set *rset)
{
    for (int i = 0; i < g_num_decrypters; ++i) {
        ensure_out_fd_opened(&g_decs[i]);
        int fd = g_decs[i].out_fd;
        if (fd < 0) continue;
        if (FD_ISSET(fd, rset)) {
            int rc = handle_one_guess_from_fd(fd, g_decs[i].out_pipe);
            if (rc < 0) {
                /* close and reopen later */
                close(g_decs[i].out_fd);
                g_decs[i].out_fd = -1;
            }
        }
    }
}

/* ------------------------------- Main ------------------------------------ */

int main(void)
{
    /* Init MTA crypto/rng */
    MTA_crypt_init();

    /* Read password length from config */
    {
        FILE *f = fopen(CONFIG_FILE, "r");
        if (!f) die("open conf.txt");
        unsigned long v = 0;
        if (fscanf(f, "%lu", &v) != 1 || v == 0 || (v % 8) != 0) {
            fclose(f);
            fprintf(stderr, "Invalid password length in %s (must be multiple of 8)\n", CONFIG_FILE);
            exit(EXIT_FAILURE);
        }
        fclose(f);
        g_pass_len = (size_t)v;
        g_key_len  = g_pass_len / 8;
    }

    /* Generate initial cycle (plaintext, key, ciphertext) */
    generate_new_cycle();

    /* Prepare subscription pipe */
    unlink(ENCRYPTER_PIPE);
    if (mkfifo(ENCRYPTER_PIPE, 0666) < 0 && errno != EEXIST) {
        die("mkfifo(encrypter_pipe)");
    }
    g_sub_rd = open(ENCRYPTER_PIPE, O_RDONLY | O_NONBLOCK);
    if (g_sub_rd < 0) die("open encrypter_pipe RD");

    /* Hold a dummy write-end so read() doesn’t go EOF when no writers present */
    g_sub_wr_hold = open(ENCRYPTER_PIPE, O_WRONLY | O_NONBLOCK);
    /* It's OK if this fails (e.g., no reader for write end yet) */

    log_message("[ENCRYPTER] Started and ready for subscriptions.");

    /* Main select loop */
    for (;;) {
        fd_set rset;
        FD_ZERO(&rset);

        int maxfd = -1;

        /* subscription pipe */
        FD_SET(g_sub_rd, &rset);
        if (g_sub_rd > maxfd) maxfd = g_sub_rd;

        /* all decrypter out_fds (for guesses) */
        for (int i = 0; i < g_num_decrypters; ++i) {
            ensure_out_fd_opened(&g_decs[i]); /* try to open if not yet */
            if (g_decs[i].out_fd >= 0) {
                FD_SET(g_decs[i].out_fd, &rset);
                if (g_decs[i].out_fd > maxfd) maxfd = g_decs[i].out_fd;
            }
        }

        int rc = select(maxfd + 1, &rset, NULL, NULL, NULL);
        if (rc < 0) {
            if (errno == EINTR) continue;
            die("select");
        }

        if (FD_ISSET(g_sub_rd, &rset)) {
            handle_subscription_readable();
        }

        handle_guesses(&rset);
    }

    /* not reached */
    return 0;
}
