#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netdb.h>

#define BUFFER_SIZE 1024
char* clientID = "2";

long getFileSize(const char *filename) {
    FILE *file = fopen(filename, "r");  // open the file in read mode
    if (file == NULL) {
        fprintf(stderr, "Failed to open file %s\n", filename);
        exit(EXIT_FAILURE);
    }
    fseek(file, 0, SEEK_END); // move the file pointer to the end of the file
    long size = ftell(file); // get the current file pointer position, which is the size
    fclose(file); // close the file
    return size - 1; // removing the newline from the file size
} // end of "getFileSize" function


void error(const char *msg) {
    perror(msg);
    exit(0);
} // end of "error" function



void setupAddressStruct(struct sockaddr_in* address, int portNumber, char* hostname){
    memset((char*) address, '\0', sizeof(*address));
    address->sin_family = AF_INET;
    address->sin_port = htons(portNumber);
    struct hostent* hostInfo = gethostbyname(hostname);
    if (hostInfo == NULL) {
        fprintf(stderr, "CLIENT: ERROR, no such host\n");
        exit(0);
    }
    memcpy((char*) &address->sin_addr.s_addr,
           hostInfo->h_addr_list[0],
           hostInfo->h_length);
} // end of "setupAddressStruct" function


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

    if (argc < 4) {
        fprintf(stderr, "USAGE: %s file key port\n", argv[0]);
        exit(0);
    }

    /*
    * creating variables to store the args passed in the terminal
    */
    char* file = argv[1];
    char* keyFile = argv[2];
    portNumber = atoi(argv[3]);

    long keySize = getFileSize(keyFile); // storing the keyFile size by calling upon the getFileSize function which returns the entire file size minus the newline char
    char* cypherText = malloc(sizeof(char) * (keySize + 1)); // key must be at least as big as plainTextMessage so allocating the key as the size
    size_t sizeofCypherText = sizeof(cypherText);
    memset(cypherText, '\0', sizeofCypherText);
    char allowedChars[28] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ "; // storing an index of allowable chars to ensure that the chars in the plain text are allowed

    FILE* fileHandler; // creating file handler
    fileHandler = fopen(file, "r"); // opening the file in read binary mode which forces no transformation of the data
    if (fileHandler == NULL) { // error handling on fileHandler
        fprintf(stderr, "File (%s) does not exist\n", file);
        exit(EXIT_FAILURE);
    }


    /*
     * Copying file contents into plainText var
     */
    char fileCharacter;
    int charCounter = 0;
    char charOfInterest;

    while (true) {
        fileCharacter = fgetc(fileHandler); // getting the char from the file
        if (fileCharacter == '\n' || fileCharacter == EOF) break; // if the char is a newline or the end of the file, stop reading
        bool validChar = false; // this boolean will track if each char is a valid one
        for (int i = 0; i < 28; i++) {
            if (fileCharacter == allowedChars[i]) { // looping through the allowed chars. If the char that we just read is in the allowedChars array, then it is a valid char. We thus set the validChar var to true and break the iteration.
                validChar = true;
                break;
            }
            charOfInterest = fileCharacter; // creating these two variables for the purposes of error testing. That way if there is an invalid char, we know which one.
        } // end of for loop (i)
        if (!validChar) { // if it is not a valid char, meaning the validChar variable was never made true, then we throw an error
            fprintf(stderr, "Error: invalid Character in plain text file %s. Character in question is %c at location \n", file, charOfInterest);
            exit(EXIT_FAILURE);
        }
        cypherText[charCounter++] = fileCharacter; // tacking on the char, once we know it's valid, and iterating the index.

        if (keySize < charCounter) { // always checking that the keySize is not less than the charCounter. If it is, it means that we need to throw and error.
            fprintf(stderr, "Error: cypher text larger than designated key.\n");
            exit(EXIT_FAILURE);
        }
    } // end of while loop (true)
    fclose(fileHandler); // closing the file handler and tacking a null terminator onto our plainTextMessage variable.

    /*
     * Creating socket and setting up connection with server
     */
    socketFD = socket(AF_INET, SOCK_STREAM, 0);
    if (socketFD < 0) error("CLIENT: ERROR opening socket");
    setupAddressStruct(&serverAddress, portNumber, "localhost");
    if (connect(socketFD, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) < 0) {
        fprintf(stderr, "DEC CLIENT: ERROR connecting\n");
        return 2;
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
     */
    char* serverIDToken = malloc(sizeof(char) * 256); // allocating mem
    size_t sizeofServerIDToken = sizeof(serverIDToken);
    memset(serverIDToken, '\0', sizeofServerIDToken); // memset the allocated memory
    charsRead = recv(socketFD, serverIDToken, 1, 0); // receiving the servers ID token. Because these are fixed sizes and both the client and the server know the size, we are using the serverIDToken to read. It is common practice however for us to use the buffer on most occasions.
    if (charsRead < 0) perror("Error reading the clientIDToken \n");
    charsWritten = send(socketFD, clientID, strlen(clientID), 0); // sending our clientID to the server.
    if (charsWritten < 0) perror("Error in writing to client the serverID\n");
    if (strcmp(serverIDToken, clientID) != 0) { // if the server ID and the client ID don't match, throw error.
        perror("Client attempted to access server that does not share access ID. Closing connection.. \n");
        close(socketFD); // close the socket
        return 2; // returning 2
    }




    /*
    *  Sending keySizeString
    * */
    memset(buffer, '\0', sizeofBuffer); // memsetting the buffer
    char* keySizeString = malloc(sizeof(char) * (BUFFER_SIZE)); // allocating memory
    size_t sizeofKeySizeString = sizeof(keySizeString);
    memset(keySizeString, '\0', sizeofKeySizeString); // memsetting allocated memory
    sprintf(keySizeString, "%ld", keySize); // copying the KeySize into the keySizeString (converting int to string)
    strcpy(buffer, keySizeString); // copying the keySize to the buffer
    charsWritten = send(socketFD, buffer, BUFFER_SIZE - 1, 0); // sending the key size to the server
    if (charsWritten < 0) perror("Error writing keySizeString to server \n");
    memset(ackToken, '\0', sizeofAckToken);
    charsRead = recv(socketFD, ackToken, 14, 0); // ensuring that I get an ack from the server




    /*
    *   Sending the keyFile to the server
    * */
    memset(buffer, '\0', sizeofBuffer); // memsetting the buffer
    strcpy(buffer, keyFile); // copying the keyFile into the buffer
    charsWritten = send(socketFD, buffer, BUFFER_SIZE - 1, 0);  // sending the keyFile to the server
    if (charsWritten < 0) fprintf(stderr, "Error writing keyFile to server \n");
    if (charsWritten < strlen(keySizeString)) fprintf(stderr, "CLIENT: WARNING: Not all data written to socket!\n");
    memset(ackToken, '\0', sizeofAckToken);
    charsRead = recv(socketFD, ackToken, lengthOfAck, 0); // ensuring that I get an ack from the server
    if (charsRead < 0) perror("Error sending the ack after sending keyFile\n");
    if (charsWritten < 0) perror("CLIENT: ERROR writing to socket");
    if (charsWritten < strlen(keySizeString)) fprintf(stderr, "CLIENT: WARNING: Not all data written to socket!\n");




    /*
     *  Sending the plainMessageSizeString to the server
     * */
    char* cypherTextMessageSizeString = malloc(sizeof(char) * (strlen(cypherText) + 1)); // allocating memory and memsetting it
    size_t sizeofCypherTextMessageSizeString = sizeof(cypherTextMessageSizeString);
    memset(cypherTextMessageSizeString, '\0', sizeofCypherTextMessageSizeString);
    memset(buffer, '\0', sizeofBuffer); // memsetting the buffer
    sprintf(cypherTextMessageSizeString, "%lu", strlen(cypherText)); // converting long to string
    strcpy(buffer, cypherTextMessageSizeString);  // copying the plainTextMessageSizeString into the buffer
    charsWritten = send(socketFD, buffer, BUFFER_SIZE - 1, 0); // sending the plainTextMessageSizeString
    if (charsWritten < 0) fprintf(stderr, "Error writing cypherTextMessageSizeString to server\n");
    memset(ackToken, '\0', sizeofAckToken);
    charsRead = recv(socketFD, ackToken, lengthOfAck, 0); // ensuring that I get an ack from the server
    if (charsRead < 0) perror("Error sending the ack after sending plainMessageSizeString\n");




    /*
     * Sending cypher
     */
    int cypherTextSize = atoi(cypherTextMessageSizeString); // storing the cypherTextSize
    int iterationsRemaining = (cypherTextSize / 100) + 1; // This value will hold the number of times I plan on looping.
    int count = 0;
    int iterations = 0;
    while (iterationsRemaining != 0) { // sending 100 chars at a time to the server to ensure there is no error
        iterations += 1;
        int numberOfCharsToSend = (iterationsRemaining == 1) ? (cypherTextSize % 100) : 100; // if iterations remaining is 1, set the numberOfCharsToSend to plainTextSize mod 100. else, set it to 100
        if (numberOfCharsToSend == 0) numberOfCharsToSend = 100; // handling the situation where the number of chars to send is a multiple of 100.

        charsWritten = send(socketFD, cypherText + count, numberOfCharsToSend, 0); // writing the cypherText + the numberOfChars we have already sent
        if (charsWritten < 0) fprintf(stderr, "CLIENT: ERROR writing cypherText to socket on iteration %d\n", count);

        memset(ackToken, '\0', sizeofBuffer);
        charsRead = recv(socketFD, ackToken, lengthOfAck, 0); // ensuring that I get an ack from the server
        char* token = malloc(sizeof(char) * numberOfCharsToSend);
        for (int i = 0; i < numberOfCharsToSend; i++) {
            token[i] = (cypherText + count)[i];
        }
        if (charsRead < 0) fprintf(stderr, "Error sending ack in sending cypher  sending iteration \n");
        if (charsWritten < 0) fprintf(stderr, "Error sending data\n");
        if (charsWritten < numberOfCharsToSend) fprintf(stderr, "CLIENT: WARNING: Not all data written to socket!\n");
        count += numberOfCharsToSend;
        iterationsRemaining--; // decrementing iterationsRemaining
        free(token);
    } // end while loop




    /*
     * Getting plain text
     */
    char* plainText = malloc(sizeof(char) * (cypherTextSize + 1)); // creating plainText var
    size_t sizePlainText = sizeof(plainText);
    memset(plainText, '\0', sizePlainText); // memsetting it to null terminators
    iterations = 0;
    iterationsRemaining = (cypherTextSize / 100) + 1; // getting the number of iterations it will take to send 100 chars at a time
    count = 0;
    while (iterationsRemaining != 0) {  // looping until there are no iterations remaining
        iterations += 1;
        int numberOfCharsToReceive = (iterationsRemaining == 1) ? (cypherTextSize % 100) : 100; // if iterations remaining is 1, set the numberOfCharsToSend to plainTextSize mod 100. else, set it to 100
        if (numberOfCharsToReceive == 0) numberOfCharsToReceive = 100; // handling the situation where the number of chars to send is a multiple of 100.

        memset(buffer, '\0', sizeofBuffer); // receiving the encrypted text via buffer
        charsRead = recv(socketFD, buffer, numberOfCharsToReceive, 0);
        if (charsRead < 0) fprintf(stderr, "Error reading plainText text from socket on iteration %d \n", iterations);
        charsWritten = send(socketFD, ack, lengthOfAck, 0); // sending ack
        strcat(plainText, buffer); // concatenating the portion of the plainText text into our plainText var
        count += numberOfCharsToReceive;
        iterationsRemaining--;
    } // end of while loop


    printf("%s\n", plainText); // outputting the plainText along with a newline to the stdout
    close(socketFD); // Close the socket
    return 0;
}