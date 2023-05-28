#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>


#define PORT 8080

int main() {
    int clientSocket;
    struct sockaddr_in serverAddress;

    // Create a socket for the client
    if ((clientSocket = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(PORT);

    // Convert IP address to binary form
    if (inet_pton(AF_INET, "127.0.0.1", &(serverAddress.sin_addr)) <= 0) {
        perror("inet_pton failed");
        exit(EXIT_FAILURE);
    }

    // Connect to the server
    if (connect(clientSocket, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) == -1) {
        perror("connect failed");
        exit(EXIT_FAILURE);
    }

    // Send and receive data through the socket
    char message[1024];
    while (1) {
        printf("Channel: Enter a message: ");
        fgets(message, sizeof(message), stdin);
        message[strcspn(message, "\n")] = '\0';  // Remove trailing newline

        // Send the message to the parent process
        send(clientSocket, message, sizeof(message), 0);

        // Check if the message is "exit" to terminate the channel
        if (strcmp(message, "exit") == 0)
            break;
    }

    // Close the socket
    close(clientSocket);

    return 0;
}
