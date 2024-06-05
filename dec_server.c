#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define BUFFER_SIZE 1024

char* serverID = "2";
char allowedChars[28] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ ";


int getCharIndex(char desiredChar) {
    for (int i = 0; i < 27; i++) {
        if (allowedChars[i] == desiredChar) return i;
    }
    fprintf(stderr, "getCharIndex was unable to locate the desired char in the allowedChars array.\n");
    return -1;
} // end of "getCharIndex" function


void error(const char *msg) {
    perror(msg);
    exit(1);
} // end of "error" function


void setupAddressStruct(struct sockaddr_in* address, int portNumber){
    memset((char*) address, '\0', sizeof(*address));
    address->sin_family = AF_INET;
    address->sin_port = htons(portNumber);
    address->sin_addr.s_addr = INADDR_ANY;
} // end of "setupAddressStruct" function


int main(int argc, char *argv[]){
    /*
    * Setting up various variables that will be used in the function
    */
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

    if (argc < 2) {
        fprintf(stderr,"USAGE: %s port\n", argv[0]);
        exit(1);
    }
    int listenSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (listenSocket < 0) {
        fprintf(stderr, "ERROR opening socket");
        exit(EXIT_FAILURE);
    }
    setupAddressStruct(&serverAddress, atoi(argv[1]));
    if (bind(listenSocket, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) < 0) error("ERROR on binding");
    listen(listenSocket, 5);

    while(true) {
        connectionSocket = accept(listenSocket, (struct sockaddr *)&clientAddress, &sizeOfClientInfo);
        if (connectionSocket < 0) {
            fprintf(stderr, "ERROR on accept");
            continue;
        }

        /*
              * Much of the reads and writes follow the following format:
              * - Create a variable
              * - memset it to null terminators
              * - memset the buffer to null terminators
              * - read or write using the buffer. In the case that we are writing, we will copy the variable into the buffer after we have memset the buffer
              * - get or receive acknowledgment
              * - if the information we need was read to the buffer, copy it to the var we want.
        */

        /*
        * Sending ID's; this portion of the code checks that the client and the server have the same ID's and therefore are allowed to communicate with one another.
        * */
        char* clientIDToken = malloc(sizeof(char) * 256); // allocating mem
        size_t sizeofClientIDToken = sizeof(clientIDToken);
        memset(clientIDToken, '\0', sizeofClientIDToken); // memset the allocated memory
        charsWritten = send(connectionSocket, serverID, strlen(serverID), 0); // sending the servers ID token. Because these are fixed sizes and both the client and the server know the size, we are using the serverIDToken to read. It is common practice however for us to use the buffer on most occasions.
        if (charsWritten < 0) perror("Error in writing to client the serverID\n");
        charsRead = recv(connectionSocket, clientIDToken, 1, 0); // getting the clientID to the server.
        if (charsRead < 0) perror("Dec Error reading the clientIDToken \n");
        if (strcmp(clientIDToken, serverID) != 0) { // if the server ID and the client ID don't match, throw error.
            perror("Client attempting to access server does not share access ID. CLosing the connection.. \n");
            close(connectionSocket); // closing the connection
            continue; // breaking out of this connection and re-running the loop.
        }




        /*
        * Getting keySizeString
        * */
        memset(buffer, '\0', sizeofBuffer); // memsetting the buffer
        charsRead = recv(connectionSocket, buffer, BUFFER_SIZE - 1, 0); // getting the length of the key
        if (charsRead < 0) perror("ERROR reading Key Size from socket");
        charsWritten = send(connectionSocket, ack, lengthOfAck, 0); // sending an ack to the client
        if (charsWritten < 0) perror("Error sending ack after receiving length of the key");

        int keyStringSize = 255;
        char* keySize = malloc(sizeof(char) * (keyStringSize + 1));
        size_t sizeofKeySize = sizeof(keySize);
        memset(keySize, '\0', sizeofKeySize); // memsetting the keySize var
        strcpy(keySize, buffer); // assigning the buffer value to the keySize



        /*
         *  Getting the keyFile from the client
         */
        memset(buffer, '\0', sizeofBuffer); // memsetting the buffer
        charsRead = recv(connectionSocket, buffer, BUFFER_SIZE - 1, 0); // getting the file path to the key
        if (charsRead < 0) fprintf(stderr, "ERROR reading Key Size from socket");

        charsWritten = send(connectionSocket, ack, 14, 0); // sending an ack to the client for the keyFile
        if (charsWritten < 0) perror("Error sending ack to the client for the keyfile\n");
        memset(keyFile, '\0', sizeofKeyFile);
        strcpy(keyFile, buffer); // assigning the buffer value to the keyFile



        /*
        *  Getting the cypherTextSize from client
        */
        memset(buffer, '\0', sizeofBuffer); // memsetting the buffer
        charsRead = recv(connectionSocket, buffer, BUFFER_SIZE - 1, 0); // getting the cypherTextSize
        if (charsRead < 0) fprintf(stderr, "Error reading plain message size from socket\n");
        charsWritten = send(connectionSocket, ack, lengthOfAck, 0); // sending an ack to the client
        int cypherTextSize = atoi(buffer); // assigning the int of the buffer to the cypherTextSize



        /*
         * Getting cypher text
         */
        int keySizeInt = atoi(keySize);
        int iterationsRemaining = (cypherTextSize / 100) + 1; // This value will hold the number of times I plan on looping.
        int count = 0;
        char* cypherText = malloc(sizeof(char) * (keySizeInt + 1));
        if (cypherText == NULL) fprintf(stderr, "Error allocating memory for cypherText \n");
        size_t sizeofCypherText = sizeof(cypherText);
        memset(cypherText, '\0', sizeofCypherText);

        int iterations = 0;
        while (iterationsRemaining != 0) { // sending 100 chars at a time to the server to ensure there is no error
            iterations += 1;
            int numberOfCharsToReceive = (iterationsRemaining == 1) ? (cypherTextSize % 100) : 100; // if iterations remaining is 1, set the numberOfCharsToSend to plainTextSize mod 100. else, set it to 100
            if (numberOfCharsToReceive == 0) numberOfCharsToReceive = 100; // handling the situation where the number of chars to send is a multiple of 100.

            memset(buffer, '\0', sizeofBuffer);
            charsRead = recv(connectionSocket, buffer, numberOfCharsToReceive, 0); // getting a portion of the cypherText from the client
            if (charsRead < 0) fprintf(stderr, "Error reading cypher from socket on iteration %d \n", iterations);
            charsWritten = send(connectionSocket, ack, lengthOfAck, 0); // sending an ack to the client
            if (charsWritten < 0) fprintf(stderr, "Error sending ack when receiving cypher text\n");

            bool containsOnlyValidChars = false;
            char charOfInterest;
            for (int i = 0; i < numberOfCharsToReceive; i++) { // looping through each char received and ensuring that it is in the valid chars array. If one of the chars received is not in the valid chars array, exit the program as the plain text is not valid.
                for (int j = 0; j < 28; j++) {
                    if (buffer[i] == allowedChars[j]) {
                        containsOnlyValidChars = true;
                        break;
                    }
                }
                if (!containsOnlyValidChars) { // error handling; letting the server know what char gave the error
                    charOfInterest = buffer[i];
                    printf("CharOfInterest: %c \n", charOfInterest);
                    fprintf(stderr, "Does not contain only valid chars \n");
                    exit(EXIT_FAILURE);
                }
            }

            strcat(cypherText, buffer); // concatenating the portion of the encrypted text into our encryptedText var
            count += numberOfCharsToReceive;
            iterationsRemaining--;
        } // end of while loop





        /*
         * Decrypting cypherText
         */
        char* keyText = malloc(sizeof(char) * (keySizeInt + 1));
        size_t sizeofKeyText = sizeof(keyText);
        memset(keyText, '\0', sizeofKeyText);

        FILE* fileHandler = fopen(keyFile, "r"); // opening the keyFile in read mode
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
        size_t sizeofPlainText = sizeof(plainText);
        memset(plainText, '\0', sizeofPlainText);
        for (int i = 0; i < strlen(cypherText); i++) { // encrypt the plainTextMessage (cypherTextIndex - keyCharIndex) % 27
            int cypherTextIndex = getCharIndex(cypherText[i]);
            int keyCharIndex = getCharIndex(keyText[i]);
            int newCharIndex = (cypherTextIndex - keyCharIndex) % 27; // encrypting the plain text char
            if (newCharIndex < 0) newCharIndex = newCharIndex + 27; // if the newCharIndex is negative, just add the 27 back to it to get the proper value
            char plainTextChar = allowedChars[newCharIndex]; // getting the plainChar
            plainText[i] = plainTextChar; // putting the plainChar into the plainText string
        } // end of for loop



        /*
         * Sending plain Text
         */
        count = 0;
        iterations = 0;
        iterationsRemaining = (cypherTextSize / 100) + 1; // getting the number of iterations it will take to send 100 chars at a time
        while (iterationsRemaining != 0) { // looping until there are no iterations remaining
            iterations += 1;
            int numberOfCharsToSend = (iterationsRemaining == 1) ? (cypherTextSize % 100) : 100; // if iterations remaining is 1, set the numberOfCharsToSend to plainTextSize mod 100. else, set it to 100
            if (numberOfCharsToSend == 0) numberOfCharsToSend = 100; // handling the situation where the number of chars to send is a multiple of 100.
            charsWritten = send(connectionSocket, plainText + count, numberOfCharsToSend, 0); // writing the plainTextMessage + the numberOfChars we have already sent
            if (charsWritten < 0) fprintf(stderr, "SERVER: ERROR writing plainText to socket on iteration %d\n", iterations);
            memset(ackToken, '\0', sizeofBuffer);
            charsRead = recv(connectionSocket, ackToken, lengthOfAck, 0);
            if (charsRead < 0) fprintf(stderr, "Error sending ack while sending plain text\n");
            count += numberOfCharsToSend;
            iterationsRemaining--; // decrementing iterationsRemaining
        } // end of while loop


        /*
         * Closing
         */
        close(connectionSocket);
    } // end of while loop
} // end of "main" function