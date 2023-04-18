#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>

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

// Use : ./client <server_ip> <server_port>

#define max_length 50
int fin_discussion = 0;
char msg [max_length];
char input[max_length]; 


void *afficher(int color, char *msg, void *args){
    
    //Remonte le curseur d'une ligne
    printf("\033[2K\r");
    printf("\033[1A");
    printf("\033[2K\r");
    //Change la couleur du texte

    printf("\033[%dm", color);
    //Affiche le message
    printf(msg, args);
    printf("\n\033[35m");
    printf("---------- Entrez un message (max %d caracteres) -----------\n", max_length - 1);

    //Change la couleur du texte en rouge
    printf("\033[1m");
    printf("Saisie : ");
    printf("\033[0m");
    fflush(stdout); //Flush le buffer de stdout
    return NULL;
}

void *readMessage(void *arg) {
    int dS = *(int *)arg;
    int nb_recv;

    while(1) {

        // Read message

        nb_recv = recv(dS, msg, max_length, 0);
        if (nb_recv == -1) {
            perror("Erreur lors de la reception du message");
            close(dS);
            exit(EXIT_FAILURE);
        } else if (nb_recv == 0) {
            // Connection closed by remote host
            afficher(31, "Connection closed by remote host\n", NULL);
            close(dS);
            exit(EXIT_FAILURE);
        }

        // Check if the message is "fin"
        // If it is, close the socket and exit
        if (strcmp(msg, "fin") == 0) {
            afficher(31, "L'autre client met fin a la discussion\n", NULL);
            fin_discussion = 1;
            break;
        } else {
            // Afficher le message reçu
            afficher(34, "Message recu: %s\n", msg);   

        }

    }

    return NULL;
}


void *writeMessage(void *arg) {
    int dS = *(int *)arg;
    int nb_send;

    afficher(31, "", NULL);

    while(1) {

        fgets(input, max_length, stdin); 
        char *pos = strchr(input, '\n');
        *pos = '\0';

        //Remonte le curseur d'une ligne
        printf("\033[1A"); 
        
        afficher(32, "Message envoye: %s\n", input);
        
        // Send message
        nb_send = send(dS, input, max_length, 0);
        if (nb_send == -1) {
            perror("Erreur lors de l'envoi du message");
            close(dS);
            exit(EXIT_FAILURE);
        } else if (nb_send == 0) {
            // Connection closed by remote host
            afficher(31, "Connection closed by remote host\n", NULL);
            close(dS);
            exit(EXIT_FAILURE);
        }

        // Check if the message is "fin"
        // If it is, close the socket and exit
        if (strcmp(input, "fin") == 0) {
            afficher(31, "Vous mettez fin a la discussion\n", NULL);
            fin_discussion = 1;
            break;
        }

    }

    return NULL;
}


void handle_sigint(int sig) {
    afficher(31, "Pour quitter, veuillez saisir le mot 'fin'.\n", NULL);
}



int main(int argc, char *argv[]) {

    if (argc != 3) {
        printf("Error: You must provide exactly 2 arguments.\n\
                Usage: %s <server_ip> <server_port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    printf("\033[2J");
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


    signal(SIGINT, handle_sigint);

    // Create threads
    pthread_t readThread;
    pthread_t writeThread;


    // Read message
    if (pthread_create(&readThread, NULL, readMessage, &dS) != 0) {
        perror("Erreur lors de la creation du thread de lecture");
        close(dS);
        exit(EXIT_FAILURE);
    }

    // Write message
    if (pthread_create(&writeThread, NULL, writeMessage, &dS) != 0) {
        perror("Erreur lors de la creation du thread d'ecriture");
        close(dS);
        exit(EXIT_FAILURE);
    }

    while(!fin_discussion) {
        sleep(1);
    }



    // Close the socket
    if (close(dS) == -1){
        perror("Erreur de close du socket");
        exit(EXIT_FAILURE);
    }

    printf("\033[2K\r");
    printf("\033[1A");
    printf("\033[2K\r");

    return EXIT_SUCCESS;
}