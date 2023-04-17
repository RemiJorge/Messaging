#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

// DOCUMENTATION
// This program acts as a client which connects to a server
// to talk with another client
// It uses the TCP protocol
// It takes two arguments, the server ip and the server port
// If at some point one of the clients sends "fin",
// the server will close the discussion between the clients
// and wait for a new one.

// IMPORTANT:
// The third argument is 1 if the client is a writer, 2 if it is a reader
// MAKE SURE THE WRITER CONNECTS FIRST

// You can use gcc to compile this program:
// gcc -o client client.c

// Use : ./client <server_ip> <server_port> <readOrWrite>

int main(int argc, char *argv[]) {

    if (argc != 4) {
        printf("Error: You must provide exactly 2 arguments.\n\
                Usage: %s <server_ip> <server_port> <readOrWrite>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    printf("Debut programme client\n");
    printf("Bienvenue sur la messagerie instantanee !\n");

    int dS = socket(PF_INET, SOCK_STREAM, 0);
    if (dS == -1) {
        perror("Erreur creation socket");
        exit(EXIT_FAILURE);
    }

    printf("Socket Créé\n");

    // Voici la doc des structs utilisées
    /*
    struct sockaddr_in {
        sa_family_t    sin_family;  famille d'adresses : AF_INET     
        uint16_t       sin_port;    port dans l'ordre d'octets réseau
        struct in_addr sin_addr;    adresse Internet                 
    };
    */

    struct sockaddr_in aS;

    aS.sin_family = AF_INET;
    int result = inet_pton(AF_INET,argv[1],&(aS.sin_addr));
    if (result == 0) {
        fprintf(stderr, "Invalid address\n");
        exit(EXIT_FAILURE);
    } else if (result == -1) {
        perror("inet_pton");
        exit(EXIT_FAILURE);
    }

    /*
        Adresse Internet
        struct in_addr {
            uint32_t    s_addr;   Adresse dans l'ordre d'octets réseau 
        };
    */
    aS.sin_port = htons(atoi(argv[2])) ;
    socklen_t lgA = sizeof(struct sockaddr_in) ;
    if (connect(dS, (struct sockaddr *) &aS, lgA) == -1) {
        perror("Erreur connect client");
        exit(EXIT_FAILURE);
    }

    printf("Socket Connecté\n");

    int readOrWrite = atoi(argv[3]) - 1; // 0 = write, 1 = read
    int max_length = 50;
    char msg [max_length];
    char input[max_length]; 
    int nb_recv;
    int nb_send;

    while(1){

        if (readOrWrite == 0) {

            // Ask for message
            // fgets() reads a line from stdin and stores it into the string pointed to by input
            printf("Entrez un message (max %d caracteres): ", max_length - 1);
            fgets(input, max_length, stdin); 
            char *pos = strchr(input, '\n');
            *pos = '\0';
            
            // Send message

            nb_send = send(dS, input, max_length, 0);
            if (nb_send == -1) {
                perror("Erreur lors de l'envoi du message");
                close(dS);
                exit(EXIT_FAILURE);
            } else if (nb_send == 0) {
                // Connection closed by remote host
                printf("Connection closed by remote host\n");
                close(dS);
                exit(EXIT_FAILURE);
            }

            // Check if the message is "fin"
            // If it is, close the socket and exit
            if (strcmp(input, "fin") == 0) {
                printf("Vous mettez fin a la discussion\n");
                break;
            }


        } else {

            // Read message

            nb_recv = recv(dS, msg, max_length, 0);
            if (nb_recv == -1) {
                perror("Erreur lors de la reception du message");
                close(dS);
                exit(EXIT_FAILURE);
            } else if (nb_recv == 0) {
                // Connection closed by remote host
                printf("Connection closed by remote host\n");
                close(dS);
                exit(EXIT_FAILURE);
            }

            // Check if the message is "fin"
            // If it is, close the socket and exit
            if (strcmp(msg, "fin") == 0) {
                printf("L'autre client met fin a la discussion\n");
                break;
            } else {
                printf("Message recu: %s\n", msg);
            }

        }

        readOrWrite = !readOrWrite;

    }

    // Close the socket
    if (close(dS) == -1){
        perror("Erreur de close du socket");
        exit(EXIT_FAILURE);
    }

    return EXIT_SUCCESS;
}