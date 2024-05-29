#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>  // ssize_t
#include <sys/socket.h> // send(),recv()
#include <netdb.h>      // gethostbyname()

/**
* Client code
* 1. Create a socket and connect to the server specified in the command arugments.
* 2. Prompt the user for input and send that input as a message to the server.
* 3. Print the message received from the server and exit the program.
*/

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
    char buffer[256];
    // Check usage & args
    if (argc < 4) {
        fprintf(stderr,"USAGE: %s hostname port\n", argv[0]);
        exit(0);
    }
    char* file = malloc(sizeof(char*) + (strlen(argv[1]) + 1)); // allocating the proper size of the file
    char* key = malloc(sizeof(char*) + (strlen(argv[2]) + 1)); // allocating the proper size of the key
    strcpy(file, argv[1]); // copying the file name from the args
    strcpy(key, argv[2]); // copying the key from args
    portNumber = atoi(argv[3]); // getting the port number
    char* plainTextMessage = malloc(sizeof (char*) + (atoi(key) + 1));
    char allowedChars[28] = "abcdefghijklmnopqrstuvwxyz ";
    FILE* fileHandler;
    char fileCharacter;
    int charCounter = 0;
    bool validChar = false;

    fileHandler = fopen(file, "rb");
    if (fileHandler == NULL) {
        fprintf(stderr, "File does not exist");
    }

    while (true) {
        validChar = false;
        fileCharacter = fgetc(fileHandler);
        if (fileCharacter == '\n' || fileCharacter == EOF) {
            if (charCounter != atoi(key)) { // if the key and plain text size are not exactly equal then return an error
                plainTextMessage = NULL; // rest the plain message
                fprintf(stderr, "Key and plain text are not equal size.\n"); // outputting the error
            }
            break;
        }
        for (int i = 0; i < 27; i++) {
            if (fileCharacter == allowedChars[i]) {
                validChar = true;
            }
        } // end of for loop (i)
        if (validChar == true) {
            plainTextMessage[charCounter++] = fileCharacter;
        }
        else {
            fprintf(stderr, "Invalid Character in plain text \n");
            break;
        }
        if (atoi(key) < charCounter) {
            fprintf(stderr, "plain text larger than designated key.\n");
            break;
        }
    } // end of while loop (true)
    fclose(fileHandler);

    // Create a socket
    socketFD = socket(AF_INET, SOCK_STREAM, 0);
    if (socketFD < 0){
        error("CLIENT: ERROR opening socket");
    }

    // Set up the server address struct
    char* hostName = "localhost";
    setupAddressStruct(&serverAddress, portNumber, hostName);

    // Connect to server
    if (connect(socketFD, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) < 0){
        error("CLIENT: ERROR connecting");
    }
    // Get input message from user
    printf("CLIENT: Enter text to send to the server, and then hit enter: ");
    // Clear out the buffer array
    memset(buffer, '\0', sizeof(buffer));
    // Get input from the user, trunc to buffer - 1 chars, leaving \0
    fgets(buffer, sizeof(buffer) - 1, stdin);
    // Remove the trailing \n that fgets adds
    buffer[strcspn(buffer, "\n")] = '\0';

    // Send message to server
    // Write to the server
    charsWritten = send(socketFD, buffer, strlen(buffer), 0);
    if (charsWritten < 0){
        error("CLIENT: ERROR writing to socket");
    }
    if (charsWritten < strlen(buffer)){
        printf("CLIENT: WARNING: Not all data written to socket!\n");
    }

    // Get return message from server
    // Clear out the buffer again for reuse
    memset(buffer, '\0', sizeof(buffer));
    // Read data from the socket, leaving \0 at end
    charsRead = recv(socketFD, buffer, sizeof(buffer) - 1, 0);
    if (charsRead < 0){
        error("CLIENT: ERROR reading from socket");
    }
    printf("CLIENT: I received this from the server: \"%s\"\n", buffer);

    // Close the socket
    close(socketFD);
    return 0;
}