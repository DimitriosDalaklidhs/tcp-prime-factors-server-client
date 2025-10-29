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
#define INT_LEN 12

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

    while (inputNumber >= p * p) {
        if (inputNumber % p == 0) {
            snprintf(intStr, INT_LEN, "%d,", p);
            if (write(cfd, intStr, strlen(intStr)) != strlen(intStr))
                errMsg("Error on write");
            inputNumber = inputNumber / p;
        } else {
            p++;
        }
    }

    if (inputNumber > 1) {
        snprintf(intStr, INT_LEN, "%d,", inputNumber);
        if (write(cfd, intStr, strlen(intStr)) != strlen(intStr))
            errMsg("Error on write");
    }

    if (write(cfd, "\n", 1) != 1)
        errMsg("Error on write");
}

void handleClient(int cfd) {
    char inputBuffer[INT_LEN];

    // Send the initial message to the client
    if (write(cfd, "Give me a positive integer\n", 27) != 27)
        errMsg("Error on write");

    // Read the input number from the client
    int bytesRead = read(cfd, inputBuffer, sizeof(inputBuffer));
    if (bytesRead <= 0) {
        errMsg("Error reading from socket");
    }

    inputBuffer[bytesRead] = '\0';

    // Convert the input to an integer
    int inputNumber = atoi(inputBuffer);

    // Check if the input is a positive integer
    if (inputNumber <= 0) {
        if (write(cfd, "Invalid input. Connection terminated.\n", 38) != 38)
            errMsg("Error on write");

        close(cfd);
        exit(EXIT_SUCCESS);
    }

    // Calculate and send prime factors
    calculatePrimeFactors(cfd, inputNumber);

    // Close the connection
    if (close(cfd) == -1)
        errMsg("close");

    exit(EXIT_SUCCESS);
}

int main() {
    int lfd, cfd;
    struct sockaddr_in serv_addr, client_addr;
    socklen_t addrlen = sizeof(client_addr);
    int reuse = 1;

    while (1) {
        lfd = socket(AF_INET, SOCK_STREAM, 0);
        if (lfd == -1)
            errMsg("socket");

        // Set SO_REUSEADDR option to avoid "bind: Address already in use" issue
        if (setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) == -1)
            errMsg("setsockopt");

        memset(&serv_addr, 0, sizeof(serv_addr));

        serv_addr.sin_family = AF_INET;
        serv_addr.sin_addr.s_addr = INADDR_ANY;

        serv_addr.sin_port = htons(PORT);

        if (bind(lfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1) {
            perror("bind");

            // Close the socket and retry with a different port
            if (close(lfd) == -1)
                errMsg("close");

            continue;  // Retry with a different port
        }

        if (listen(lfd, 5) == -1)
            errMsg("listen");

        printf("Server listening on port %d\n", PORT);

        cfd = accept(lfd, (struct sockaddr *)&client_addr, &addrlen);

        if (cfd == -1) {
            errMsg("accept");
            close(lfd);
            continue;  // Retry with a different port
        }

        if (fork() == 0) {
            // child process
            close(lfd); // child doesn't need the listener
            handleClient(cfd);
            exit(EXIT_SUCCESS); // exit after handling the client
        } else {
            // parent process
            close(cfd); // parent doesn't need the client socket
        }
    }

    return 0;
}
