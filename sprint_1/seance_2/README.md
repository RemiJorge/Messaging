# Message Relay Server Between Two Clients

PLEASE READ THE DOCUMENTATION FOR MORE INFORMATION

This is a simple messaging server written in C that relays text messages
between two clients using the TCP protocol. The server and the clients are multi-threaded.

For now, the server can only handle two clients.
If they both disconnect, the server will stop.

We have improved the graphical interface of the client!
Please enjoy the new features!

## Compilation

To compile the server and the client, run the following command:
./compil.sh

## IMPORTANT:

Launch the server first.

=> ./server port

Then the clients

=> ./client ip port 

=> ./client ip port 

To disconnect from the server, send the message "fin" to the server.

For now, the server can only handle two clients.
If they both disconnect, the server will stop.
