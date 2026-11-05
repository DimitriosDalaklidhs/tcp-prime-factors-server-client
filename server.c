/*
 * TCP prime-factors server.
 *
 * Protocol (one request per connection, newline-terminated text):
 *   server -> client   "Give me a positive integer\n"
 *   client -> server   "<n>\n"            with 1 <= n <= INT_MAX
 *   server -> client   "2,2,3,7\n"        or an error line, then close
 *
 * Concurrency: one forked child per connection, at most MAX_CHILDREN at once.
 */
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>

#define PORT               8080
#define MAX_CHILDREN       64   /* concurrent clients before we reply "busy"   */
#define CLIENT_TIMEOUT_SEC 30   /* total time one client may hold a child      */
#define LINE_LEN           64   /* input buffer; longer lines are rejected     */
#define REPLY_LEN          128  /* longest reply, for 2^30, is 60 chars        */

static const char PROMPT[]     = "Give me a positive integer\n";
static const char INVALID[]    = "Invalid input. Please send a positive integer.\n";
static const char NO_FACTORS[] = "1 has no prime factors\n";
static const char BUSY[]       = "Server busy, try again later\n";

/* Number of live children. main() only changes it with SIGCHLD blocked, and
 * the SIGCHLD handler is the only other writer, so updates never race. */
static volatile sig_atomic_t activeChildren = 0;

static void errMsg(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

static void installHandler(int sig, void (*handler)(int), int flags) {
    struct sigaction sa;

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = flags;
    if (sigaction(sig, &sa, NULL) == -1)
        errMsg("sigaction");
}

/* Reap every child that has exited, without blocking. */
static void sigchldHandler(int sig) {
    int savedErrno = errno;  /* waitpid() must not clobber errno in main() */

    (void)sig;
    while (waitpid(-1, NULL, WNOHANG) > 0)
        activeChildren--;
    errno = savedErrno;
}

/* SIGTERM/SIGINT in the parent. It holds no state that needs cleaning up,
 * so it exits straight from the handler (write() and _exit() are
 * async-signal-safe). As PID 1 in a container this is what lets
 * `docker stop` finish immediately instead of waiting 10 s for SIGKILL. */
static void stopHandler(int sig) {
    static const char msg[] = "Shutting down\n";

    (void)sig;
    if (write(STDOUT_FILENO, msg, sizeof(msg) - 1) < 0) {
        /* nothing more we can do in a signal handler */
    }
    _exit(EXIT_SUCCESS);
}

/* SIGALRM in a child: the client used up its CLIENT_TIMEOUT_SEC. */
static void timeoutHandler(int sig) {
    static const char msg[] = "Client timed out\n";

    (void)sig;
    if (write(STDERR_FILENO, msg, sizeof(msg) - 1) < 0) {
        /* nothing more we can do in a signal handler */
    }
    _exit(EXIT_FAILURE);
}

/* write() may send fewer bytes than asked; keep going until all are sent. */
static int writeAll(int fd, const char *buf, size_t len) {
    while (len > 0) {
        ssize_t n = write(fd, buf, len);
        if (n == -1) {
            if (errno == EINTR)
                continue;
            return -1;
        }
        buf += n;
        len -= (size_t)n;
    }
    return 0;
}

/* TCP is a byte stream, so one read() can return part of a line. Read until
 * '\n', EOF or a full buffer. One byte per read() never consumes anything past
 * the newline, and lines here are tiny. Returns bytes read, or -1 on error. */
static ssize_t readLine(int fd, char *buf, size_t cap) {
    size_t len = 0;

    while (len < cap - 1) {
        ssize_t n = read(fd, buf + len, 1);
        if (n == -1) {
            if (errno == EINTR)
                continue;
            return -1;
        }
        if (n == 0)
            break;                      /* client closed its side */
        if (buf[len++] == '\n')
            break;
    }
    buf[len] = '\0';
    return (ssize_t)len;
}

/* Accept "84", "84\n" or "84\r\n" with 1 <= value <= INT_MAX. Anything else,
 * including trailing characters such as "12abc", is rejected. */
static int parsePositiveInt(const char *s, size_t len, int *out) {
    char *end;
    long value;

    errno = 0;
    value = strtol(s, &end, 10);
    if (errno != 0 || end == s || value <= 0 || value > INT_MAX)
        return -1;
    if (*end == '\r')
        end++;
    if (end != s + len && *end != '\n')
        return -1;
    *out = (int)value;
    return 0;
}

static int appendFactor(char *buf, size_t cap, size_t *len, int factor) {
    int w = snprintf(buf + *len, cap - *len, "%s%d", *len > 0 ? "," : "", factor);

    if (w < 0 || (size_t)w >= cap - *len)
        return -1;                      /* would truncate */
    *len += (size_t)w;
    return 0;
}

/* Write the prime factors of n (n >= 2) into buf as "2,2,3,7\n".
 * Trial division only needs p <= sqrt(n). Testing p <= n / p instead of
 * p * p <= n gives the same result without overflowing int near INT_MAX. */
static int formatPrimeFactors(int n, char *buf, size_t cap) {
    size_t len = 0;

    for (int p = 2; p <= n / p; p++) {
        while (n % p == 0) {
            if (appendFactor(buf, cap, &len, p) == -1)
                return -1;
            n /= p;
        }
    }
    if (n > 1 && appendFactor(buf, cap, &len, n) == -1)
        return -1;
    if (len + 2 > cap)
        return -1;
    buf[len] = '\n';
    buf[len + 1] = '\0';
    return 0;
}

/* Serve one client. Runs in the child; returns the child's exit status. */
static int handleClient(int cfd) {
    char line[LINE_LEN];
    char reply[REPLY_LEN];
    const char *out;
    ssize_t len;
    int n;

    if (writeAll(cfd, PROMPT, sizeof(PROMPT) - 1) == -1) {
        perror("write prompt");
        return EXIT_FAILURE;
    }

    len = readLine(cfd, line, sizeof(line));
    if (len == -1) {
        perror("read");
        return EXIT_FAILURE;
    }
    if (len == 0) {
        fprintf(stderr, "Client disconnected before sending input\n");
        return EXIT_FAILURE;
    }

    /* A full buffer that doesn't end in '\n' means the line was too long. */
    int tooLong = (size_t)len == sizeof(line) - 1 && line[len - 1] != '\n';

    if (tooLong || parsePositiveInt(line, (size_t)len, &n) == -1) {
        out = INVALID;
    } else if (n == 1) {
        out = NO_FACTORS;
    } else if (formatPrimeFactors(n, reply, sizeof(reply)) == 0) {
        out = reply;
    } else {
        fprintf(stderr, "Reply buffer too small for %d\n", n);  /* see REPLY_LEN */
        return EXIT_FAILURE;
    }

    if (writeAll(cfd, out, strlen(out)) == -1) {
        perror("write reply");
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

/* Errors from accept() usually concern one connection, not the server. */
static void handleAcceptError(int err) {
    if (err == EINTR)
        return;
    fprintf(stderr, "accept: %s\n", strerror(err));
    if (err == EBADF || err == EINVAL || err == ENOTSOCK || err == EFAULT)
        exit(EXIT_FAILURE);   /* the listening socket itself is broken */
    if (err == EMFILE || err == ENFILE || err == ENOBUFS || err == ENOMEM)
        sleep(1);             /* out of resources: back off instead of spinning */
    /* Anything else (e.g. ECONNABORTED): that client is gone, keep serving. */
}

int main(void) {
    struct sockaddr_in servAddr;
    sigset_t chldMask, prevMask;
    int lfd, cfd;
    int reuse = 1;

    /* stdout is fully buffered when it isn't a terminal (Docker, redirects).
     * Line-buffer it so log lines show up immediately and no unflushed output
     * gets copied into every forked child. */
    setvbuf(stdout, NULL, _IOLBF, 0);

    installHandler(SIGCHLD, sigchldHandler, SA_RESTART | SA_NOCLDSTOP);
    installHandler(SIGTERM, stopHandler, 0);
    installHandler(SIGINT, stopHandler, 0);
    installHandler(SIGPIPE, SIG_IGN, 0);  /* get EPIPE from write() instead of dying */

    sigemptyset(&chldMask);
    sigaddset(&chldMask, SIGCHLD);

    lfd = socket(AF_INET, SOCK_STREAM, 0);
    if (lfd == -1)
        errMsg("socket");

    /* Allow quick restart without "Address already in use" errors */
    if (setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) == -1)
        errMsg("setsockopt");

    memset(&servAddr, 0, sizeof(servAddr));
    servAddr.sin_family      = AF_INET;
    servAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servAddr.sin_port        = htons(PORT);

    if (bind(lfd, (struct sockaddr *)&servAddr, sizeof(servAddr)) == -1)
        errMsg("bind");

    if (listen(lfd, SOMAXCONN) == -1)
        errMsg("listen");

    printf("Server listening on port %d\n", PORT);

    for (;;) {
        cfd = accept(lfd, NULL, NULL);
        if (cfd == -1) {
            handleAcceptError(errno);
            continue;
        }

        if (activeChildren >= MAX_CHILDREN) {
            writeAll(cfd, BUSY, sizeof(BUSY) - 1);  /* best effort */
            close(cfd);
            continue;
        }

        /* Block SIGCHLD so the child can't be reaped (and counted down)
         * before the parent has counted it up. */
        sigprocmask(SIG_BLOCK, &chldMask, &prevMask);

        pid_t pid = fork();
        if (pid == -1) {
            perror("fork");
            writeAll(cfd, BUSY, sizeof(BUSY) - 1);  /* best effort */
            close(cfd);
        } else if (pid == 0) {
            /* Child: default signal behaviour, a deadline, one client.
             * _exit() rather than exit() so stdio buffers inherited from
             * the parent are never flushed a second time. */
            installHandler(SIGCHLD, SIG_DFL, 0);
            installHandler(SIGTERM, SIG_DFL, 0);
            installHandler(SIGINT, SIG_DFL, 0);
            installHandler(SIGALRM, timeoutHandler, 0);
            sigprocmask(SIG_SETMASK, &prevMask, NULL);
            close(lfd);

            alarm(CLIENT_TIMEOUT_SEC);
            int status = handleClient(cfd);
            close(cfd);
            _exit(status);
        } else {
            activeChildren++;
            close(cfd);
        }

        sigprocmask(SIG_SETMASK, &prevMask, NULL);
    }
}
