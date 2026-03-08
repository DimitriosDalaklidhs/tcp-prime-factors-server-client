#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

#define PORT 8080
#define INT_LEN 13   /* enough for 12 digits + null terminator */

void errMsg(const char *msg);
void calculatePrimeFactors(int cfd, int inputNumber);
void handleClient(int cfd);

void errMsg(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

void calculatePrimeFactors(int cfd, int inputNumber) {
    char intStr[INT_LEN];
    int p = 2;
    int first = 1;  /* track first factor to avoid leading comma */

    while (inputNumber >= p * p) {
        if (inputNumber % p == 0) {
            /* Print separator before all factors except the first */
            if (!first) {
                if (write(cfd, ",", 1) != 1)
                    errMsg("Error on write");
            }
            first = 0;

            snprintf(intStr, INT_LEN, "%d", p);
            if (write(cfd, intStr, strlen(intStr)) != (ssize_t)strlen(intStr))
                errMsg("Error on write");

            inputNumber = inputNumber / p;
        } else {
            p++;
        }
    }

    if (inputNumber > 1) {
        if (!first) {
            if (write(cfd, ",", 1) != 1)
                errMsg("Error on write");
        }
        snprintf(intStr, INT_LEN, "%d", inputNumber);
        if (write(cfd, intStr, strlen(intStr)) != (ssize_t)strlen(intStr))
            errMsg("Error on write");
    }

    if (write(cfd, "\n", 1) != 1)
        errMsg("Error on write");
}

void handleClient(int cfd) {
    /* Buffer sized to hold INT_LEN - 1 chars + null terminator safely */
    char inputBuffer[INT_LEN];
    ssize_t bytesRead;

    /* Send the initial prompt to the client */
    const char *prompt = "Give me a positive integer\n";
    if (write(cfd, prompt, strlen(prompt)) != (ssize_t)strlen(prompt))
        errMsg("Error on write");

    /* Read the input number from the client.
     * Leave one byte for the null terminator to avoid OOB write. */
    bytesRead = read(cfd, inputBuffer, sizeof(inputBuffer) - 1);
    if (bytesRead <= 0) {
        if (bytesRead == 0)
            fprintf(stderr, "Client disconnected before sending input.\n");
        else
            perror("Error reading from socket");
        close(cfd);
        exit(EXIT_FAILURE);
    }

    inputBuffer[bytesRead] = '\0';

    /* Use strtol for robust parsing with error detection */
    char *endptr;
    errno = 0;
    long inputNumber = strtol(inputBuffer, &endptr, 10);

    if (errno != 0 || endptr == inputBuffer || inputNumber <= 0 || inputNumber > INT_MAX) {
        const char *errResp = "Invalid input. Please send a positive integer.\n";
        write(cfd, errResp, strlen(errResp));  /* best-effort; ignore return */
        close(cfd);
        exit(EXIT_SUCCESS);
    }

    /* Calculate and send prime factors */
    calculatePrimeFactors(cfd, (int)inputNumber);

    if (close(cfd) == -1)
        errMsg("close");

    exit(EXIT_SUCCESS);
}

int main(void) {
    int lfd, cfd;
    struct sockaddr_in serv_addr, client_addr;
    socklen_t addrlen = sizeof(client_addr);
    int reuse = 1;

    /* Create the listening socket once, outside the accept loop */
    lfd = socket(AF_INET, SOCK_STREAM, 0);
    if (lfd == -1)
        errMsg("socket");

    /* Allow quick restart without "Address already in use" errors */
    if (setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) == -1)
        errMsg("setsockopt");

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family      = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port        = htons(PORT);

    if (bind(lfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1)
        errMsg("bind");

    if (listen(lfd, 5) == -1)
        errMsg("listen");

    printf("Server listening on port %d\n", PORT);

    while (1) {
        /* Reap any finished child processes to avoid zombies */
        while (waitpid(-1, NULL, WNOHANG) > 0)
            ;

        cfd = accept(lfd, (struct sockaddr *)&client_addr, &addrlen);
        if (cfd == -1) {
            /* EINTR can happen if a signal interrupted accept — just retry */
            if (errno == EINTR)
                continue;
            errMsg("accept");
        }

        pid_t pid = fork();
        if (pid == -1) {
            perror("fork");
            close(cfd);
            continue;
        }

        if (pid == 0) {
            /* Child: handle the client, then exit */
            close(lfd);
            handleClient(cfd);
            /* handleClient exits internally, but be explicit */
            exit(EXIT_SUCCESS);
        } else {
            /* Parent: close the client fd and go back to accepting */
            close(cfd);
        }
    }

    /* Unreachable, but tidy */
    close(lfd);
    return 0;
}
