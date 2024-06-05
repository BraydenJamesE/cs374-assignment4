#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>  // ssize_t
#include <sys/socket.h> // send(),recv()
#include <netdb.h>      // gethostbyname()


#define BUFFER_SIZE 1024
char* clientID = "1";

long getFileSize(const char *filename) {
    FILE *file = fopen(filename, "r");  // open the file in read mode
    if (file == NULL) { // error handling
        fprintf(stderr, "Failed to open file %s\n", filename);
        exit(EXIT_FAILURE);
    }
    fseek(file, 0, SEEK_END);// move the file pointer to the end of the file
    long size = ftell(file);  // get the current file pointer position, which is the size
    fclose(file); //Close the file
    return size - 1; // removing the newline from the file size
} // end of "getFileSize" function


void error(const char *msg) {
    perror(msg);
    exit(0);
} // end of "error" function


void setupAddressStruct(struct sockaddr_in* address, int portNumber, char* hostname){ // this function was taken from the resources that were in the assignment.
    memset((char*) address, '\0', sizeof(*address));
    address->sin_family = AF_INET;
    address->sin_port = htons(portNumber);
    struct hostent* hostInfo = gethostbyname(hostname);
    if (hostInfo == NULL) {
        fprintf(stderr, "CLIENT: ERROR, no such host\n");
        exit(0);
    }
    memcpy((char*)&address->sin_addr.s_addr,
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

    if (argc < 4) {
        fprintf(stderr, "USAGE: %s file key port\n", argv[0]);
        exit(0);
    }

    char* file = argv[1]; // allocating the proper size of the file
    char* keyFile = argv[2]; // creating a filename variable that holds that maxsize of 256; this is the maximum size a file name can be in linux including the null terminator
    portNumber = atoi(argv[3]); // getting the port number

    long keySize = getFileSize(keyFile);
    char* plainTextMessage = malloc(sizeof(char) + (keySize + 1)); // key must be at least as big as plainTextMessage so allocating the key as the size
    size_t sizeofPlainTextMessage = sizeof(plainTextMessage);
    memset(plainTextMessage, '\0', sizeofPlainTextMessage);
    char allowedChars[28] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ ";

    FILE* fileHandler; // creating file handler
    fileHandler = fopen(file, "r");
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
    int charOfInterestLocation = 0;

    while (true) {
        fileCharacter = fgetc(fileHandler); // getting the char from the file
        if (fileCharacter == '\n' || fileCharacter == EOF) break; // if the char is a newline or the end of the file, stop reading
        bool validChar = false;
        for (int i = 0; i < 28; i++) { // looping through the allowed chars. If the char that we just read is in the allowedChars array, then it is a valid char. We thus set the validChar var to true and break the iteration.
            if (fileCharacter == allowedChars[i]) {
                validChar = true;
                break;
            }
            charOfInterest = fileCharacter; // creating these two variables for the purposes of error testing. That way if there is an invalid char, we know which one.
            charOfInterestLocation = i;
        } // end of for loop (i)
        if (!validChar) { // if it is not a valid char, meaning the validChar variable was never made true, then we throw an error
            fprintf(stderr, "Invalid Character in plain text in file %s. Character in question is %c at location %d \n", file, charOfInterest, charOfInterestLocation);
            break;
        }
        plainTextMessage[charCounter++] = fileCharacter;

        if (keySize < charCounter) {
            fprintf(stderr, "plain text larger than designated key.\n");
            exit(EXIT_FAILURE);
        }
    } // end of while loop (true)
    fclose(fileHandler);



    /*
     * Creating socket and setting up connection with server
     */
    socketFD = socket(AF_INET, SOCK_STREAM, 0);
    if (socketFD < 0){
        error("CLIENT: ERROR opening socket");
    }
    setupAddressStruct(&serverAddress, portNumber, "localhost");
    if (connect(socketFD, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) < 0) {
        fprintf(stderr, "CLIENT: ERROR connecting\n");
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
     * Sending ID's
     * */
    char* serverIDToken = malloc(sizeof(char) * 256);
    size_t sizeofServerIDToken = sizeof(serverIDToken);
    memset(serverIDToken, '\0', sizeofServerIDToken);
    charsRead = recv(socketFD, serverIDToken, 1, 0); // receiving the servers ID token. Because these are fixed sizes and both the client and the server know the size, we are using the serverIDToken to read. It is common practice however for us to use the buffer on most occasions.
    if (charsRead < 0) perror("Error reading the clientIDToken \n");
    charsWritten = send(socketFD, clientID, strlen(clientID), 0); // sending our clientID to the server.
    if (charsWritten < 0) perror("Error in writing to client the serverID\n");
    if (strcmp(serverIDToken, clientID) != 0) { // if the server ID and the client ID don't match, throw error.
        fprintf(stderr, "Client attempted to access server that does not share access ID. Closing connection.. \n");
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
    sprintf(keySizeString, "%ld", keySize); // copying the KeySize into the keySizeString (converting int to string)
    strcpy(buffer, keySizeString);
    charsWritten = send(socketFD, buffer, BUFFER_SIZE - 1, 0); // sending the key size to the server
    if (charsWritten < 0) perror("Error writing keySizeString to server \n");
    memset(ackToken, '\0', sizeofAckToken);
    charsRead = recv(socketFD, ackToken, lengthOfAck, 0); // ensuring that I get an ack from the server




    /*
     *  Sending the keyFile to the server
     * */
    memset(buffer, '\0', sizeofBuffer);
    strcpy(buffer, keyFile);
    charsWritten = send(socketFD, buffer, BUFFER_SIZE - 1, 0);  // sending the keyFile to the server
    if (charsWritten < 0) fprintf(stderr, "Error writing keyFile to server \n"); // error handling
    if (charsWritten < strlen(keySizeString)) fprintf(stderr, "CLIENT: WARNING: Not all data written to socket!\n");
    memset(ackToken, '\0', sizeofAckToken);
    charsRead = recv(socketFD, ackToken, lengthOfAck, 0); // ensuring that I get an ack from the server
    if (charsRead < 0) perror("Error sending the ack after sending keyFile\n");




    /*
     *  Sending the plainMessageSizeString to the server
     * */
    char* plainTextMessageSizeString = malloc(sizeof(char) * (strlen(plainTextMessage) + 1));
    size_t sizeofPlainTextMessageSizeString = sizeof(plainTextMessageSizeString);
    memset(plainTextMessageSizeString, '\0', sizeofPlainTextMessageSizeString);
    memset(buffer, '\0', sizeofBuffer);
    sprintf(plainTextMessageSizeString, "%lu", strlen(plainTextMessage)); // converting long to string for sending
    strcpy(buffer, plainTextMessageSizeString); // copying the plainTextMessageSizeString into the buffer
    charsWritten = send(socketFD, buffer, BUFFER_SIZE - 1, 0);
    if (charsWritten < 0) fprintf(stderr, "Error writing plainMessageSizeString to server\n");
    memset(ackToken, '\0', sizeofAckToken);
    charsRead = recv(socketFD, ackToken, lengthOfAck, 0); // ensuring that I get an ack from the server
    if (charsRead < 0) perror("Error sending the ack after sending plainMessageSizeString\n");




    /*
     * Sending PlainText
     */
    int plainTextSize = atoi(plainTextMessageSizeString);
    int iterationsRemaining = (plainTextSize / 100) + 1; // This value will hold the number of times I plan on looping.
    int count = 0;
    int iterations = 0;
    while (iterationsRemaining != 0) { // sending 100 chars at a time to the server to ensure there is no error
        iterations += 1;
        int numberOfCharsToSend = (iterationsRemaining == 1) ? (plainTextSize % 100) : 100; // if iterations remaining is 1, set the numberOfCharsToSend to plainTextSize mod 100. else, set it to 100
        if (numberOfCharsToSend == 0) {
            numberOfCharsToSend = 100; // handling the situation where the number of chars to send is a multiple of 100.
        }

        charsWritten = send(socketFD, plainTextMessage + count, numberOfCharsToSend, 0);
        if (charsWritten < 0) fprintf(stderr, "CLIENT: ERROR writing plaintextMessage to socket on iteration %d\n", count);
        memset(ackToken, '\0', sizeofBuffer);
        charsRead = recv(socketFD, ackToken, lengthOfAck, 0); // ensuring that I get an ack from the server
        if (charsRead < 0) fprintf(stderr, "Error sending ack in plainText sending iteration \n");

        char* token = malloc(sizeof(char) * numberOfCharsToSend);
        for (int i = 0; i < numberOfCharsToSend; i++) {
            token[i] = (plainTextMessage + count)[i];
        }

        if (charsWritten < 0) fprintf(stderr, "Error sending data\n");

        if (charsWritten < numberOfCharsToSend) fprintf(stderr, "CLIENT: WARNING: Not all data written to socket!\n");
        count += numberOfCharsToSend; // incrementing the count by the amount of chars written
        iterationsRemaining--; // decrementing iterationsRemaining
        free(token);
    } // end while loop




    /*
     * Getting encrypted Text
     */
    char* encryptedText = malloc(sizeof(char) * (plainTextSize + 1));
    size_t sizeofEncryptedText = sizeof(encryptedText);
    memset(encryptedText, '\0', sizeofEncryptedText);
    iterations = 0;
    iterationsRemaining = (plainTextSize / 100) + 1; // getting the number of iterations it will take to send 100 chars at a time
    count = 0;
    while (iterationsRemaining != 0) { // looping until there are no iterations remaining
        iterations += 1;
        int numberOfCharsToReceive = (iterationsRemaining == 1) ? (plainTextSize % 100) : 100; // if iterations remaining is 1, set the numberOfCharsToSend to plainTextSize mod 100. else, set it to 100
        if (numberOfCharsToReceive == 0) numberOfCharsToReceive = 100; // handling the situation where the number of chars to send is a multiple of 100.

        memset(buffer, '\0', sizeofBuffer);
        charsRead = recv(socketFD, buffer, numberOfCharsToReceive, 0); // reading parts of the encrypted text
        if (charsRead < 0) fprintf(stderr, "Error reading encrypted text from socket on iteration %d \n", iterations);
        charsWritten = send(socketFD, ack, lengthOfAck, 0); // sending ack
        strcat(encryptedText, buffer);
        count += numberOfCharsToReceive;
        iterationsRemaining -= 1;
    } // end of while loop


    printf("%s\n", encryptedText); // writing encrypted text to file
    close(socketFD); // Close the socket
    return 0;
}