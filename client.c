#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8080
#define BUFFER_SIZE 256  // Buffer size for receiving data (sufficient for simple text messages)

// Prints error message and exits program
void errMsg(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

int main(int argc, char *argv[]) {

    // Expect exactly one argument: server IP address
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <server_ip>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // Create TCP socket
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1)
        errMsg("socket");

    // Prepare server address structure
    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));  // Zero out structure to avoid garbage values

    serv_addr.sin_family = AF_INET;            // IPv4
    serv_addr.sin_port = htons(PORT);          // Convert port to network byte order

    // Convert IP string to binary form
    if (inet_pton(AF_INET, argv[1], &serv_addr.sin_addr) <= 0)
        errMsg("inet_pton");

    // Establish connection to server
    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1) {
        fprintf(stderr, "Error: Unable to connect to the server.\n");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    char buffer[BUFFER_SIZE];

    // ---- Step 1: Receive greeting from server ----
    int bytesRead = read(sockfd, buffer, sizeof(buffer) - 1);
    if (bytesRead <= 0) {
        errMsg("Error reading greeting from server");
    }

    buffer[bytesRead] = '\0';  // Null-terminate received string
    printf("Server: %s", buffer);

    // Verify server follows expected text-based protocol:
    // it must prompt us with a specific message before we send input
    if (strncmp(buffer, "Give me a positive integer", 26) != 0) {
        fprintf(stderr, "Unexpected server message.\n");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    // ---- Step 2: Get user input and send to server ----
    int positiveInteger;
    printf("Give me a positive integer\n");

    // Read integer from user (NOTE: no validation, assumes correct numeric input)
    scanf("%d", &positiveInteger);

    // Convert integer to string (text protocol); newline marks end of message
    char sendBuf[32];
    int len = snprintf(sendBuf, sizeof(sendBuf), "%d\n", positiveInteger);

    // Send the string to the server
    if (write(sockfd, sendBuf, len) != len) {
        errMsg("Error writing integer to socket");
    }

    // ---- Step 3: Receive and display server response ----
    // Read until server closes connection (read() blocks until data arrives or socket closes)
    // Expected: prime factorization result from server
    while ((bytesRead = read(sockfd, buffer, sizeof(buffer) - 1)) > 0) {
        buffer[bytesRead] = '\0';
        printf("%s", buffer);
    }

    // Clean up socket
    close(sockfd);

    return 0;
}
