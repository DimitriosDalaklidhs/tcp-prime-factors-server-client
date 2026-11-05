/*
 * TCP prime-factors client.
 *
 * Usage: ./client <server_ip>
 * Waits for the server's prompt, sends one positive integer and prints the
 * prime factors the server sends back.
 */
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#define PORT        8080
#define BUFFER_SIZE 256

static const char PROMPT[] = "Give me a positive integer\n";

static void errMsg(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
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
 * '\n', EOF or a full buffer. Returns bytes read, or -1 on error. */
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
            break;                      /* server closed the connection */
        if (buf[len++] == '\n')
            break;
    }
    buf[len] = '\0';
    return (ssize_t)len;
}

/* Read a number from 1 to INT_MAX from stdin, asking again on bad input.
 * Returns 0 on success, -1 if stdin ends first. */
static int readPositiveInt(int *out) {
    char line[64];

    while (fgets(line, sizeof(line), stdin) != NULL) {
        int complete = strchr(line, '\n') != NULL || feof(stdin);

        if (!complete) {                /* over-long line: drop the rest */
            int c;
            while ((c = getchar()) != '\n' && c != EOF)
                ;
        }

        char *end;
        errno = 0;
        long value = strtol(line, &end, 10);
        while (isspace((unsigned char)*end))
            end++;

        if (complete && errno == 0 && end != line && *end == '\0' &&
            value > 0 && value <= INT_MAX) {
            *out = (int)value;
            return 0;
        }
        printf("Please enter a whole number from 1 to %d\n", INT_MAX);
        fflush(stdout);
    }
    return -1;
}

int main(int argc, char *argv[]) {
    struct sockaddr_in servAddr;
    char buffer[BUFFER_SIZE];
    char sendBuf[32];
    ssize_t n;
    int sockfd, number, len, rc;

    if (argc != 2) {
        fprintf(stderr, "Usage: %s <server_ip>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    /* If the server closes first, write() should fail with EPIPE instead of
     * SIGPIPE silently killing the client. */
    signal(SIGPIPE, SIG_IGN);

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1)
        errMsg("socket");

    memset(&servAddr, 0, sizeof(servAddr));
    servAddr.sin_family = AF_INET;
    servAddr.sin_port   = htons(PORT);

    /* inet_pton() returns 0 for a malformed address without setting errno */
    rc = inet_pton(AF_INET, argv[1], &servAddr.sin_addr);
    if (rc == 0) {
        fprintf(stderr, "Not a valid IPv4 address: %s\n", argv[1]);
        exit(EXIT_FAILURE);
    }
    if (rc == -1)
        errMsg("inet_pton");

    if (connect(sockfd, (struct sockaddr *)&servAddr, sizeof(servAddr)) == -1)
        errMsg("connect");

    /* ---- Step 1: the server must greet us with the expected prompt ---- */
    n = readLine(sockfd, buffer, sizeof(buffer));
    if (n == -1)
        errMsg("read");
    if (n == 0) {
        fprintf(stderr, "Server closed the connection without a prompt\n");
        exit(EXIT_FAILURE);
    }
    if (strcmp(buffer, PROMPT) != 0) {
        buffer[strcspn(buffer, "\r\n")] = '\0';
        fprintf(stderr, "Unexpected server message: %s\n", buffer);
        exit(EXIT_FAILURE);
    }
    fputs(buffer, stdout);
    fflush(stdout);

    /* ---- Step 2: read a valid number and send it, newline-terminated ---- */
    if (readPositiveInt(&number) == -1) {
        fprintf(stderr, "No number entered\n");
        exit(EXIT_FAILURE);
    }
    len = snprintf(sendBuf, sizeof(sendBuf), "%d\n", number);
    if (writeAll(sockfd, sendBuf, (size_t)len) == -1)
        errMsg("write");

    /* ---- Step 3: print everything until the server closes the connection ---- */
    while ((n = read(sockfd, buffer, sizeof(buffer))) > 0)
        fwrite(buffer, 1, (size_t)n, stdout);
    if (n == -1)
        errMsg("read");

    close(sockfd);
    return 0;
}
