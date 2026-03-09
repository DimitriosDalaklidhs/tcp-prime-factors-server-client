#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8080
#define BUFFER_SIZE 256

void errMsg(const char *msg);

void errMsg(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <server_ip>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1)
        errMsg("socket");

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, argv[1], &serv_addr.sin_addr) <= 0)
        errMsg("inet_pton");

    // Connect to server
    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1) {
        fprintf(stderr, "Error: Unable to connect to the server.\n");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    char buffer[BUFFER_SIZE];

    // Read server greeting: "Give me a positive integer\n"
    int bytesRead = read(sockfd, buffer, sizeof(buffer) - 1);
    if (bytesRead <= 0) {
        errMsg("Error reading greeting from server");
    }

    buffer[bytesRead] = '\0';
    printf("Server: %s", buffer);

    // If server does not send expected greeting, exit
    if (strncmp(buffer, "Give me a positive integer", 26) != 0) {
        fprintf(stderr, "Unexpected server message.\n");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    // ----------- Sending text instead of binary int -------------
    int positiveInteger;
    printf("Give me a positive integer\n");
    scanf("%d", &positiveInteger);
    char sendBuf[32];

    int len = snprintf(sendBuf, sizeof(sendBuf), "%d\n", positiveInteger);

    if (write(sockfd, sendBuf, len) != len) {
        errMsg("Error writing integer to socket");
    }

    // ---------------------------------------------------------------------------

    // Read and print server response (prime factors)
    while ((bytesRead = read(sockfd, buffer, sizeof(buffer) - 1)) > 0) {
        buffer[bytesRead] = '\0';
        printf("%s", buffer);
    }

    close(sockfd);

    return 0;
}
