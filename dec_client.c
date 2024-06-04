#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>  // ssize_t
#include <sys/socket.h> // send(),recv()
#include <netdb.h>      // gethostbyname()


#define BUFFER_SIZE 1024
char* clientID = "2";

long getFileSize(const char *filename) {
    FILE *file = fopen(filename, "r");  // open the file in read mode
    if (file == NULL) {
        fprintf(stderr, "Failed to open file %s\n", filename);
        exit(EXIT_FAILURE);
    }

    fseek(file, 0, SEEK_END);  // move the file pointer to the end of the file
    long size = ftell(file);   // get the current file pointer position, which is the size
    fclose(file);              // Close the file
    return size - 1; // removing the newline from the file size
} // end of "getFileSize" function


// Error function used for reporting issues
void error(const char *msg) {
    perror(msg);
    exit(0);
}

// Set up the address struct
void setupAddressStruct(struct sockaddr_in* address, int portNumber, char* hostname){
    // Clear out the address struct
    memset((char*) address, '\0', sizeof(*address));

    // The address should be network capable
    address->sin_family = AF_INET;
    // Store the port number
    address->sin_port = htons(portNumber);

    // Get the DNS entry for this host name
    struct hostent* hostInfo = gethostbyname(hostname);
    if (hostInfo == NULL) {
        fprintf(stderr, "CLIENT: ERROR, no such host\n");
        exit(0);
    }
    // Copy the first IP address from the DNS entry to sin_addr.s_addr
    memcpy((char*) &address->sin_addr.s_addr,
           hostInfo->h_addr_list[0],
           hostInfo->h_length);
}

int main(int argc, char *argv[]) {
    int socketFD, portNumber, charsWritten, charsRead;
    struct sockaddr_in serverAddress;
    char buffer[BUFFER_SIZE];
    size_t sizeofBuffer = sizeof(buffer);
    char ack[15] = "ACK: received.";
    int lengthOfAck = strlen(ack);
    int ackTokenSize = 255;
    char* ackToken = malloc(sizeof(char) * (ackTokenSize + 1));
    size_t sizeofAckToken = sizeof(ackToken);

    // Check usage & args
    if (argc < 4) {
        fprintf(stderr, "USAGE: %s file key port\n", argv[0]);
        exit(0);
    }

    char* file = argv[1]; // allocating the proper size of the file
    char* keyFile = argv[2]; // creating a filename variable that holds that maxsize of 256; this is the maximum size a file name can be in linux including the null terminator
    portNumber = atoi(argv[3]); // getting the port number

    long keySize = getFileSize(keyFile);
    char* cypherText = calloc(keySize + 1, sizeof(char)); // key must be at least as big as plainTextMessage so allocating the key as the size
    char allowedChars[28] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ ";

    FILE* fileHandler;
    fileHandler = fopen(file, "rb");
    if (fileHandler == NULL) {
        fprintf(stderr, "File (%s) does not exist\n", file);
        exit(EXIT_FAILURE);
    }


    /*
     * Copying file contents into plainText var
     */
    char fileCharacter;
    int charCounter = 0;
    char charOfInterest;
    int charOfInterestLocation = 0;

    while (true) {
        fileCharacter = fgetc(fileHandler);
        if (fileCharacter == '\n' || fileCharacter == EOF) break;
        bool validChar = false;
        for (int i = 0; i < 28; i++) {
            if (fileCharacter == allowedChars[i]) {
                validChar = true;
                break;
            }
            charOfInterest = fileCharacter;
            charOfInterestLocation = i;
        } // end of for loop (i)
        if (!validChar) {

            fprintf(stderr, "Invalid Character in plain text. Character in question is %c at location %d \n", charOfInterest, charOfInterestLocation);
            break;
        }
        cypherText[charCounter++] = fileCharacter;

        if (keySize < charCounter) {
            fprintf(stderr, "cypher text larger than designated key.\n");
            exit(EXIT_FAILURE);
        }
    } // end of while loop (true)
    fclose(fileHandler);
    cypherText[charCounter] = '\0';

    // Create a socket
    socketFD = socket(AF_INET, SOCK_STREAM, 0);
    if (socketFD < 0){
        error("CLIENT: ERROR opening socket");
    }

    // Set up the server address struct
    setupAddressStruct(&serverAddress, portNumber, "localhost");

    // Connect to server
    if (connect(socketFD, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) < 0) {
        fprintf(stderr, "CLIENT: ERROR connecting\n");
        return 2;
    }



    /*
     * Sending ID's
     * */
    char* serverIDToken = malloc(sizeof(char) * 256);
    size_t sizeofServerIDToken = sizeof(serverIDToken);
    memset(serverIDToken, '\0', sizeofServerIDToken);
    charsRead = recv(socketFD, serverIDToken, 1, 0);
    if (charsRead < 0) perror("Error reading the clientIDToken \n");
    charsWritten = send(socketFD, clientID, strlen(clientID), 0);
    if (charsWritten < 0) perror("Error in writing to client the serverID\n");
    if (strcmp(serverIDToken, clientID) != 0) {
        perror("Client attempted to access server that does not share access ID. Closing connection.. \n");
        close(socketFD); // close the socket
        return 2; // returning 2
    }




    /*
    *  Sending keySizeString
    * */
    memset(buffer, '\0', sizeofBuffer);
    char* keySizeString = malloc(sizeof(char) * (BUFFER_SIZE));
    size_t sizeofKeySizeString = sizeof(keySizeString);
    memset(keySizeString, '\0', sizeofKeySizeString);
    sprintf(keySizeString, "%ld", keySize);
    strcpy(buffer, keySizeString);
    charsWritten = send(socketFD, buffer, BUFFER_SIZE - 1, 0); // sending the key size to the server
    if (charsWritten < 0) perror("Error writing keySizeString to server \n");
    memset(ackToken, '\0', sizeofAckToken);
    charsRead = recv(socketFD, ackToken, 14, 0); // ensuring that I get an ack from the server




    /*
    *   Sending the keyFile to the server
    * */
    memset(buffer, '\0', sizeofBuffer);
    strcpy(buffer, keyFile);
    charsWritten = send(socketFD, buffer, BUFFER_SIZE - 1, 0);  // sending the keyFile to the server
    if (charsWritten < 0) fprintf(stderr, "Error writing keyFile to server \n");
    if (charsWritten < strlen(keySizeString)) fprintf(stderr, "CLIENT: WARNING: Not all data written to socket!\n");
    memset(ackToken, '\0', sizeofAckToken);
    charsRead = recv(socketFD, ackToken, lengthOfAck, 0); // ensuring that I get an ack from the server
    if (charsRead < 0) perror("Error sending the ack after sending keyFile\n");


    if (charsWritten < 0) {
        error("CLIENT: ERROR writing to socket");
    }
    if (charsWritten < strlen(keySizeString)) fprintf(stderr, "CLIENT: WARNING: Not all data written to socket!\n");




    /*
     *  Sending the plainMessageSizeString to the server
     * */
    char* cypherTextMessageSizeString = malloc(sizeof(char) * (strlen(cypherText) + 1));
    size_t sizeofCypherTextMessageSizeString = sizeof(cypherTextMessageSizeString);
    memset(cypherTextMessageSizeString, '\0', sizeofCypherTextMessageSizeString);
    memset(buffer, '\0', sizeofBuffer);
    sprintf(cypherTextMessageSizeString, "%lu", strlen(cypherText));
    strcpy(buffer, cypherTextMessageSizeString);
    charsWritten = send(socketFD, buffer, BUFFER_SIZE - 1, 0);
    if (charsWritten < 0) fprintf(stderr, "Error writing cypherTextMessageSizeString to server\n");
    memset(ackToken, '\0', sizeofAckToken);
    charsRead = recv(socketFD, ackToken, lengthOfAck, 0); // ensuring that I get an ack from the server
    if (charsRead < 0) perror("Error sending the ack after sending plainMessageSizeString\n");




    /*
     * Sending cypher
     */
    int cypherTextSize = atoi(cypherTextMessageSizeString);
    int iterationsRemaining = (cypherTextSize / 100) + 1; // This value will hold the number of times I plan on looping.
    int count = 0;
    int iterations = 0;
    while (iterationsRemaining != 0) { // sending 100 chars at a time to the server to ensure there is no error
        iterations += 1;
        int numberOfCharsToSend = (iterationsRemaining == 1) ? (cypherTextSize % 100) : 100;
        if (numberOfCharsToSend == 0) {
            numberOfCharsToSend = 100; // handling the situation where the number of chars to send is a multiple of 100.
        }

        charsWritten = send(socketFD, cypherText + count, numberOfCharsToSend, 0);
        if (charsWritten < 0) fprintf(stderr, "CLIENT: ERROR writing cypherText to socket on iteration %d\n", count);

        memset(ackToken, '\0', sizeofBuffer);
        charsRead = recv(socketFD, ackToken, lengthOfAck, 0); // ensuring that I get an ack from the server
        if (charsRead < 0) fprintf(stderr, "Error sending ack in sending cypher  sending iteration \n");
        char* token = malloc(sizeof(char) * numberOfCharsToSend);
        for (int i = 0; i < numberOfCharsToSend; i++) {
            token[i] = (cypherText + count)[i];
        }

        if (charsWritten < 0) fprintf(stderr, "Error sending data\n");

        if (charsWritten < numberOfCharsToSend) fprintf(stderr, "CLIENT: WARNING: Not all data written to socket!\n");
        count += numberOfCharsToSend;
        iterationsRemaining--; // decrementing iterationsRemaining
        free(token);
    } // end while loop




    /*
     * Getting plain text
     */
    char* plainText = malloc(sizeof(char) * (cypherTextSize + 1));
    size_t sizePlainText = sizeof(plainText);
    memset(plainText, '\0', sizePlainText);
    iterations = 0;
    iterationsRemaining = (cypherTextSize / 100) + 1;
    count = 0;
    while (iterationsRemaining != 0) {
        iterations += 1;
        int numberOfCharsToReceive = (iterationsRemaining == 1) ? (cypherTextSize % 100) : 100;
        if (numberOfCharsToReceive == 0) numberOfCharsToReceive = 100;

        memset(buffer, '\0', sizeofBuffer);
        charsRead = recv(socketFD, buffer, numberOfCharsToReceive, 0);
        if (charsRead < 0) fprintf(stderr, "Error reading plainText text from socket on iteration %d \n", iterations);
        charsWritten = send(socketFD, ack, lengthOfAck, 0); // sending ack
        strcat(plainText, buffer);
        count += numberOfCharsToReceive;
        iterationsRemaining--;
    } // end of while loop


    printf("%s\n", plainText);
    close(socketFD); // Close the socket
    free(plainText);
    return 0;
}