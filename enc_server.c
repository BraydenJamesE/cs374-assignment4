#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

// Error function used for reporting issues
void error(const char *msg) {
    perror(msg);
    exit(1);
}

// Set up the address struct for the server socket
void setupAddressStruct(struct sockaddr_in* address, int portNumber){

    // Clear out the address struct
    memset((char*) address, '\0', sizeof(*address));

    // The address should be network capable
    address->sin_family = AF_INET;
    // Store the port number
    address->sin_port = htons(portNumber);
    // Allow a client at any address to connect to this server
    address->sin_addr.s_addr = INADDR_ANY;
}

int main(int argc, char *argv[]){
    int connectionSocket, charsRead;
    char buffer[256];
    struct sockaddr_in serverAddress, clientAddress;
    socklen_t sizeOfClientInfo = sizeof(clientAddress);

    // Check usage & args
    if (argc < 2) {
        fprintf(stderr,"USAGE: %s port\n", argv[0]);
        exit(1);
    }

    // Create the socket that will listen for connections
    int listenSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (listenSocket < 0) {
        error("ERROR opening socket");
    }

    // Set up the address struct for the server socket
    setupAddressStruct(&serverAddress, atoi(argv[1]));

    // Associate the socket to the port
    if (bind(listenSocket, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) < 0){
        error("ERROR on binding");
    }

    // Start listening for connetions. Allow up to 5 connections to queue up
    listen(listenSocket, 5);

    // Accept a connection, blocking if one is not available until one connects
    while(true) {
        // Accept the connection request which creates a connection socket
        connectionSocket = accept(listenSocket, (struct sockaddr *)&clientAddress, &sizeOfClientInfo);
        if (connectionSocket < 0){
            error("ERROR on accept");
        }

        printf("SERVER: Connected to client running at host %d port %d\n", ntohs(clientAddress.sin_addr.s_addr), ntohs(clientAddress.sin_port));

        // Get the message from the client and display it
        memset(buffer, '\0', 256);
        int keyStringSize = 100;
        char* key = malloc(sizeof(char) * (keyStringSize + 1));
        // Read the client's message from the socket
        charsRead = recv(connectionSocket, key, keyStringSize, 0); // sending the length of the message (key)

        if (charsRead < 0){
            error("ERROR reading from socket");
        }
        printf("SERVER: I received this from the client: \"%s\"\n", key);

        // Send a Success message back to the client
        charsRead = send(connectionSocket, key, strlen(key), 0);
        int keyInt = atoi(key);
        int iterationsRemaining = (keyInt / 100) + 1; // getting the number of iterations that it will take to send over the entire message in chunks of 100
        int count = 0;
        char* plainTextMessage = malloc(sizeof(char) * (strlen(key) + 1));
        while (iterationsRemaining != 0) {
            iterationsRemaining--;
            int numberOfCharsToReceive = 0;
            if (iterationsRemaining == 0) {
                numberOfCharsToReceive = keyInt % 100;
                if (numberOfCharsToReceive == 0) {
                    numberOfCharsToReceive = 100;
                }
            }
            else {
                numberOfCharsToReceive = 100;
            }
            char* token = malloc(sizeof(char) * (numberOfCharsToReceive + 1));
            charsRead = recv(connectionSocket, token, numberOfCharsToReceive, 0);
            strcat(plainTextMessage, token);
            free(token);
            count += numberOfCharsToReceive;
        } // end of while loop
        plainTextMessage[strlen(plainTextMessage)] = '\0'; // tacking on the null terminator

        printf("Plain Message: %s \n", plainTextMessage);

        if (charsRead < 0){
            error("ERROR writing to socket");
        }

        // Close the connection socket for this client
        close(connectionSocket);
    } // end of while loop
    // Close the listening socket
    close(listenSocket);
    return 0;
}
