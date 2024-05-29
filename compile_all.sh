#!/bin/bash

# Compile enc_server.c and run the resulting executable
gcc -std=gnu99 -o enc_server enc_server.c

# Compile enc_client.c and run the resulting executable
gcc -std=gnu99 -o enc_client enc_client.c

# Compile dec_server.c and run the resulting executable
gcc -std=gnu99 -o dec_server dec_server.c

# Compile dec_client.c and run the resulting executable
gcc -std=gnu99 -o dec_client dec_client.c

# Compile keygen.c and run the resulting executable
gcc -std=gnu99 -o keygen keygen.c

