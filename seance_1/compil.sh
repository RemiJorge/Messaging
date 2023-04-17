#!/bin/bash
mkdir -p bin
gcc -o bin/client src/client.c
gcc -o bin/server src/server.c