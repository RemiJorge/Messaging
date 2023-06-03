# Message Relay Server Between Two Clients

PLEASE READ THE DOCUMENTATION FOR MORE INFORMATION

This is a simple messaging server written in C that relays text messages
between multiple clients using the TCP protocol. The server and the clients are multi-threaded.

The server is able to handle multiple clients.

We have improved the graphical interface of the client!
Please enjoy the new features!

## Compilation

To compile the server and the client, run the following command:
./compil.sh

## IMPORTANT:

**BEFORE EXECUTION, MAKE SURE YOU ARE IN THE BIN FOLDER**

Launch the server first.

=> ./server port

Then the clients

=> ./client ip port 


To disconnect from the server, send the message "fin" to the server.
