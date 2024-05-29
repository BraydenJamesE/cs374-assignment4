#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main(int argc, char** argv) {
    srand(time(NULL)); // seeding random
    char keyValues[28] = "abcdefghijklmnopqrstuvwxyz "; // creating the string that will be used to find the key text. These chars will be selected by random index
    if (argc == 2) { // ensuring expected length of args before using them; ensures proper input from user and ensures that we are not out of bounds when accessing various elements of argv
        int keySize = atoi(argv[1]); // converting the key to an int
        char keyString[keySize + 1]; // creating a variable to store the key string
        for (int i = 0; i < keySize; i++) {
            keyString[i] = keyValues[rand() % 27]; // placing key char at a random index
        } // end of for loop
        keyString[keySize] = '\0'; // tacking the null terminator onto the keyString
        printf("%s\n", keyString); // printing the keyString with a new line to the file specified
    }
    else {
        fprintf(stderr, "Error in inputting the key.\n"); // outputting the error message to stderr instead of the file path
        return 1;
    }
    return 0;
} // end of "main" function