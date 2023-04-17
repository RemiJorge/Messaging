# Message Relay Server Between Two Clients

This is a simple messaging server written in C that relays text messages
between two clients using the TCP protocol.

## Compilation

To compile the server and the client, run the following command:
./compil.sh

## IMPORTANT:

Launch the server first.

=> ./server port

Then the client WHO SENDS THE MESSAGE.

=> ./client ip port 1

MAKE SURE THIRD ARGUMENT IS 1

Then the client WHO RECEIVES THE MESSAGE.

=> ./client ip port 2
    
MAKE SURE THIRD ARGUMENT IS 2

To end a conversation, type "fin" in the client who sends the message.
The server will then wait for new clients to connect.