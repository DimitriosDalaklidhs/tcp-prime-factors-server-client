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

    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1) {
        fprintf(stderr, "Error: Unable to connect to the server.\n");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    char buffer[BUFFER_SIZE];
    int bytesRead = read(sockfd, buffer, sizeof(buffer) - 1);
    if (bytesRead <= 0) {
        errMsg("Error reading from socket");
    }

    buffer[bytesRead] = '\0';

    // Check for the correct server hello message
    if (strcmp(buffer, "Give me a positive integer\n") != 0) {
        fprintf(stderr, "Error: Unexpected server message\n");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    // Send a positive integer to the server
    int positiveInteger = 42;  // You can change this to any positive integer
    if (write(sockfd, &positiveInteger, sizeof(positiveInteger)) != sizeof(positiveInteger)) {
        errMsg("Error writing to socket");
    }

    // Read and print messages from the server until it terminates
    while ((bytesRead = read(sockfd, buffer, sizeof(buffer) - 1)) > 0) {
        buffer[bytesRead] = '\0';
        printf("%s", buffer);
    }

    close(sockfd);

    return 0;
}
