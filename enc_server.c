#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define BUFFER_SIZE 1024

char* serverID = "1";

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
    size_t sizeofBuffer = sizeof(char) * BUFFER_SIZE;
    char ack[15] = "ACK: received.";
    int lengthOfAck = strlen(ack);
    int ackTokenSize = 255;
    char* ackToken = malloc(sizeof(char) * (ackTokenSize + 1));
    int keyFileSize = 255;
    char* keyFile = malloc(sizeof(char) * keyFileSize + 1);
    size_t sizeofKeyFile = sizeof(keyFile);
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




        /*
         * Authenticating server and client ID's
         */
        char* clientIDToken = malloc(sizeof(char) * 256);
        size_t sizeofClientIDToken = sizeof(clientIDToken);
        memset(clientIDToken, '\0', sizeofClientIDToken);
        charsWritten = send(connectionSocket, serverID, strlen(serverID), 0);
        if (charsWritten < 0) perror("Error in writing to client the serverID\n");
        charsRead = recv(connectionSocket, clientIDToken, 1, 0);
        if (charsRead < 0) perror("Error reading the clientIDToken \n");
        if (strcmp(clientIDToken, serverID) != 0) {
            fprintf(stderr, "ClientIDToken: %s\n", clientIDToken);
            fprintf(stderr, "Client attempting to access server does not share access ID. CLosing the connection.. \n");
            close(connectionSocket); // closing the connection
            continue; // breaking out of this connection and re-running the loop.
        }




        /*
         * Getting keySizeString
         * */
        memset(buffer, '\0', sizeofBuffer);
        charsRead = recv(connectionSocket, buffer, BUFFER_SIZE - 1, 0); // getting the length of the key
        if (charsRead < 0) perror("ERROR reading Key Size from socket");
        charsWritten = send(connectionSocket, ack, lengthOfAck, 0); // sending an ack to the client
        if (charsWritten < 0) perror("Error sending ack after receiving length of the key");

        int keyStringSize = 255;
        char* keySize = malloc(sizeof(char) * (keyStringSize + 1));
        size_t sizeofKeySize = sizeof(keySize);
        memset(keySize, '\0', sizeofKeySize);
        strcpy(keySize, buffer); // assigning the buffer value to the keySize
        fprintf(stderr, "keySize: %s\n", keySize);




        /*
         *  Getting the keyFile from the client
         */
        memset(buffer, '\0', sizeofBuffer);
        charsRead = recv(connectionSocket, buffer, BUFFER_SIZE - 1, 0); // getting the file path to the key
        fprintf(stderr,"KeyFile: |%s|\n", buffer);
        if (charsRead < 0) fprintf(stderr, "ERROR reading Key Size from socket\n");

        charsWritten = send(connectionSocket, ack, 14, 0); // sending an ack to the client for the keyFile
        if (charsWritten < 0) perror("Error sending ack to the client for the keyfile\n");
        memset(keyFile, '\0', sizeofKeyFile);
        strcpy(keyFile, buffer);




        /*
         *  Getting the plainMessageSizeString from client
         */
        memset(buffer, '\0', sizeofBuffer);
        charsRead = recv(connectionSocket, buffer, BUFFER_SIZE - 1, 0);
        if (charsRead < 0) fprintf(stderr, "Error reading plain message size from socket\n");
        charsWritten = send(connectionSocket, ack, lengthOfAck, 0); // sending an ack to the client
        if (charsWritten < 0) perror("Error sending ack after reading plain message size");
        int plainMessageSize = atoi(buffer);
        fprintf(stderr, "PlainMessageSize: %d\n", plainMessageSize);




        /*
         * Getting plainText
         */
        int keySizeInt = atoi(keySize);
        int iterationsRemaining = (plainMessageSize / 100) + 1; // getting the number of iterations that it will take to send over the entire message in chunks of 100
        int count = 0;
        char* plainTextMessage = malloc(sizeof(char) * (keySizeInt + 1));
        if (plainTextMessage == NULL) fprintf(stderr, "Error allocating memory for plainTextMessage\n");
        size_t sizeofPlainTextMessage = sizeof(plainTextMessage);
        memset(plainTextMessage, '\0', sizeofPlainTextMessage);

        int iterations = 0;
        while (iterationsRemaining != 0) {
            iterations += 1;
            int numberOfCharsToReceive = (iterationsRemaining == 1) ? (plainMessageSize % 100) : 100;
            if (numberOfCharsToReceive == 0) {
                numberOfCharsToReceive = 100;
            }
            memset(buffer, '\0', sizeofBuffer);
            charsRead = recv(connectionSocket, buffer, numberOfCharsToReceive, 0); // getting a portion of the plain message from the client
            if (charsRead < 0) fprintf(stderr, "Error reading plaintext from socket on iteration %d \n", iterations);
            charsWritten = send(connectionSocket, ack, lengthOfAck, 0); // sending an ack to the client
            if (charsWritten < 0) fprintf(stderr, "Error sending ack after reading plaintext from socket on iteration %d", iterations);

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

            strcat(plainTextMessage, buffer);
            count += numberOfCharsToReceive;
            iterationsRemaining--;
        } // end of while loop




        /*
         * Encrypting plainText
         */
        char* keyText = malloc(sizeof(char) * (keySizeInt + 1));
        size_t sizeofKeyText = sizeof(keyText);
        memset(keyText, '\0', sizeofKeyText);

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

        if (strlen(plainTextMessage) > keySizeInt) { // checking bounds before accessing chars in the keyText in the loop
            fprintf(stderr, "Key size less than plainText size. Error.\n");
            exit(EXIT_FAILURE);
        }

        char* encryptedText = malloc(sizeof(char) * (strlen(plainTextMessage) + 1)); // creating the encryptedText string
        size_t sizeofEncryptedText = sizeof(encryptedText);
        memset(encryptedText, '\0', sizeofEncryptedText);
        for (int i = 0; i < strlen(plainTextMessage); i++) { // encrypt the plainTextMessage
            int plainCharIndex = getCharIndex(plainTextMessage[i]);
            int keyCharIndex = getCharIndex(keyText[i]);
            int newCharIndex = (plainCharIndex + keyCharIndex) % 27; // encrypting the plain text char
            char encryptedChar = allowedChars[newCharIndex];
            encryptedText[i] = encryptedChar;
        } // end of for loop




        /*
         * Sending Encrypted Text
         */
        count = 0;
        iterations = 0;
        iterationsRemaining = (plainMessageSize / 100) + 1;
        while (iterationsRemaining != 0) {
            iterations += 1;
            int numberOfCharsToSend = (iterationsRemaining == 1) ? (plainMessageSize % 100) : 100;
            if (numberOfCharsToSend == 0) numberOfCharsToSend = 100; // handling the situation where the number of chars to send is a multiple of 100.
            charsWritten = send(connectionSocket, encryptedText + count, numberOfCharsToSend, 0);
            if (charsWritten < 0) fprintf(stderr, "SERVER: ERROR writing encrypted to socket on iteration %d\n", iterations);
            memset(ackToken, '\0', sizeofBuffer);
            charsRead = recv(connectionSocket, ackToken, lengthOfAck, 0);
            if (charsRead < 0) fprintf(stderr, "Error receiving ack while sending encrypted text\n");
            count += numberOfCharsToSend;
            iterationsRemaining--; // decrementing iterationsRemaining
        } // end of while loop


        /*
         * Closing
         */
        close(connectionSocket);
    } // end of while loop
} // end of "main" function