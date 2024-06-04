#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define BUFFER_SIZE 1024

char allowedChars[28] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ ";

void error(const char *msg) {
    perror(msg);
    exit(1);
} // end of "error" function


int getCharIndex(char desiredChar) {
    for (int i = 0; i < 27; i++) {
        if (allowedChars[i] == desiredChar) return i;
    }
    fprintf(stderr, "getCharIndex was unable to locate the desired char in the allowedChars array.\n");
    return -1;
} // end of "getCharIndex" function


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
    int connectionSocket, charsRead, charsWritten;
    char buffer[BUFFER_SIZE];
    char ack[15] = "ACK: received.";
    char* keyFile = malloc(sizeof(char) * 256);
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
        fprintf(stderr, "ERROR opening socket");
        exit(EXIT_FAILURE);
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
            fprintf(stderr, "ERROR on accept");
        }

        printf("SERVER: Connected to client running at host %d port %d\n", ntohs(clientAddress.sin_addr.s_addr), ntohs(clientAddress.sin_port));

        // Get the message from the client and display it

        memset(buffer, '\0', 256);
        // reading the Key Size from the socket
        charsRead = recv(connectionSocket, buffer, BUFFER_SIZE - 1, 0); // getting the length of the key
        if (charsRead < 0){
            error("ERROR reading Key Size from socket");
        }
        buffer[charsRead] = '\0';
        charsWritten = send(connectionSocket, ack, 14, 0); // sending an ack to the client

        int keyStringSize = 255;
        char* keySize = malloc(sizeof(char) * (keyStringSize + 1));
        strcpy(keySize, buffer); // assigning the buffer value to the keySize
        printf("SERVER: I received keyStringSize from the client: \"%s\"\n", keySize);

        memset(buffer, '\0', 256);
        charsRead = recv(connectionSocket, buffer, BUFFER_SIZE - 1, 0); // getting the file path to the key
        if (charsRead < 0) fprintf(stderr, "ERROR reading Key Size from socket");

        buffer[charsRead] = '\0';
        charsWritten = send(connectionSocket, ack, 14, 0); // sending an ack to the client for the keyFile
        strcpy(keyFile, buffer);
        printf("Received the keyFile from the client: %s \n", keyFile);


        memset(buffer, '\0', BUFFER_SIZE);
        charsRead = recv(connectionSocket, buffer, BUFFER_SIZE - 1, 0);
        if (charsRead < 0) {
            fprintf(stderr, "Error reading plain message size from socket\n");
        }
        buffer[charsRead] = '\0';
        charsWritten = send(connectionSocket, ack, 14, 0); // sending an ack to the client
        printf("SERVER: I received cypherTextSize from the client: %s \n", buffer);
        int cypherTextSize = atoi(buffer);


        int keySizeInt = atoi(keySize);
        int iterationsRemaining = (cypherTextSize / 100) + 1; // getting the number of iterations that it will take to send over the entire message in chunks of 100
        int count = 0;
        char* cypherText = calloc(keySizeInt + 1, sizeof(char));
        if (cypherText == NULL) fprintf(stderr, "Error allocating memory for cypherText \n");

        int iterations = 0;
        while (iterationsRemaining != 0) {
            iterations += 1;
            int numberOfCharsToReceive = (iterationsRemaining == 1) ? (cypherTextSize % 100) : 100;
            if (numberOfCharsToReceive == 0) {
                numberOfCharsToReceive = 100;
            }
            memset(buffer, '\0', BUFFER_SIZE);
            charsRead = recv(connectionSocket, buffer, numberOfCharsToReceive, 0); // getting a portion of the plain message from the client
            if (charsRead < 0) {
                fprintf(stderr, "Error reading cypher from socket on iteration %d \n", iterations);
            }
            charsWritten = send(connectionSocket, ack, 14, 0); // sending an ack to the client
            printf("SERVER: I received cypher Text (iteration: %d) from the client: %s \n", iterations, buffer);
            printf("Expected Chars Read: %d\n", numberOfCharsToReceive);
            printf("Actual Chars Read: %d\n", charsRead);

            bool containsOnlyValidChars = false;
            char charOfInterest;
            for (int i = 0; i < numberOfCharsToReceive; i++) { // looping through each char received and ensuring that it is in the valid chars array. If one of the chars received is not in the valid chars array, exit the program as the plain text is not valid.
                for (int j = 0; j < 28; j++) {
                    if (buffer[i] == allowedChars[j]) {
                        containsOnlyValidChars = true;
                        break;
                    }
                }
                if (!containsOnlyValidChars) {
                    charOfInterest = buffer[i];
                    printf("CharOfInterest: %c \n", charOfInterest);
                    fprintf(stderr, "Does not contain only valid chars \n");
                    exit(EXIT_FAILURE);
                }
            }

            strcat(cypherText, buffer);
            count += numberOfCharsToReceive;
            iterationsRemaining--;
        } // end of while loop

        cypherText[strlen(cypherText)] = '\0'; // tacking on the null terminator

        printf("cypherText: %s \n", cypherText);

        char* keyText = malloc(sizeof(char) * (keySizeInt + 1));
        FILE* fileHandler = fopen(keyFile, "r");
        if (fileHandler == NULL) {
            fprintf(stderr, "Failed to open keyFile: %s \n", keyFile);
            exit(EXIT_FAILURE);
        }

        char ch;
        int index = 0;
        while (true) { // reading the contents of the key into a variable
            ch = fgetc(fileHandler);
            if (ch == '\n' || ch == '\0' || index > keySizeInt) break;
            else keyText[index++] = ch;
        } // end of while loop

        if (strlen(cypherText) > keySizeInt) { // checking bounds before accessing chars in the keyText in the loop
            fprintf(stderr, "Key size less than cypherText size. Error.\n");
            exit(EXIT_FAILURE);
        }
        char* plainText = malloc(sizeof(char) * (strlen(cypherText) + 1)); // creating the plainText string
        for (int i = 0; i < strlen(cypherText); i++) { // encrypt the plainTextMessage
            int cypherTextIndex = getCharIndex(cypherText[i]);
            int keyCharIndex = getCharIndex(keyText[i]);
            int newCharIndex = (cypherTextIndex - keyCharIndex) % 27; // encrypting the plain text char
            if (newCharIndex < 0) newCharIndex = newCharIndex + 27;
            char plainTextChar = allowedChars[newCharIndex];
            plainText[i] = plainTextChar;
            printf("\n\n%d: \n", i);
            printf("cypherTextIndex: %d\n", cypherTextIndex);
            printf("keyCharIndex: %d\n", keyCharIndex);
            printf("newCharIndex: %d\n", newCharIndex);
            printf("plainTextChar: %c\n", plainTextChar);
        } // end of for loop
        plainText[strlen(plainText)] = '\0';
        printf("PlainText: %s\n", plainText);

        // send plainText text
        count = 0;
        iterations = 0;
        iterationsRemaining = (cypherTextSize / 100) + 1;
        char* ackToken = malloc(sizeof(char) * (15));
        while (iterationsRemaining != 0) {
            iterations += 1;
            int numberOfCharsToSend = (iterationsRemaining == 1) ? (cypherTextSize % 100) : 100;
            if (numberOfCharsToSend == 0) numberOfCharsToSend = 100; // handling the situation where the number of chars to send is a multiple of 100.
            charsWritten = send(connectionSocket, plainText + count, numberOfCharsToSend, 0);
            if (charsWritten < 0) fprintf(stderr, "SERVER: ERROR writing plainText to socket on iteration %d\n", iterations);
            memset(ackToken, '\0', 15);
            charsRead = recv(connectionSocket, ackToken, 14, 0);
            count += numberOfCharsToSend;
            iterationsRemaining--; // decrementing iterationsRemaining
        } // end of while loop


        printf("plainText Text: %s \n", plainText);

        // Close the connection socket for this client
        close(connectionSocket);
    } // end of while loop
    // Close the listening socket
    close(listenSocket);
    return 0;
}