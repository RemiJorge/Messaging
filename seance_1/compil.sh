#!/bin/bash
mkdir -p bin
gcc -o bin/client client.c
gcc -o bin/server server.c