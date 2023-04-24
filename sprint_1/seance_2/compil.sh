#!/bin/bash
mkdir -p bin
gcc -Wall -o bin/client src/client.c
gcc -Wall -o bin/server src/server.c