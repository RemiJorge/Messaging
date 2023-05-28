#!/bin/bash
mkdir -p bin
gcc -Wall -o bin/client src/client.c
gcc -Wall -o bin/server src/server.c
gcc -Wall -o bin/client_salon src/client_salon.c