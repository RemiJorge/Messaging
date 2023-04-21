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
#define buffer_size 50
#define max_length 38
#define pseudo_length 10
char msg [buffer_size];
char input[max_length];
char pseudo[pseudo_length];


void *afficher(int color, char *msg, void *args){
    //Fonction formatant l'affichage
    
    //Efface la ligne
    printf("\033[2K\r");
    //Remonte le curseur d'une ligne
    printf("\033[1A");
    //Efface la ligne et place le curseur en debut de ligne
    printf("\033[2K\r");
    //Change la couleur du texte
    printf("\033[%dm", color);
    //Affiche le message
    printf(msg, args);
    //Change la couleur du texte en rouge
    printf("\n\033[35m");
    printf("---------- Entrez un message (max %d caracteres) -----------\n", max_length - 1);
    //Met le texte en gras
    printf("\033[1m");
    printf("Saisie : ");
    printf("\033[0m"); //Remet le texte en normal
    fflush(stdout); //Flush le buffer de stdout
    return NULL;
}


void *readMessage(void *arg) {
    // Fonction qui lit les messages envoyés par le serveur
    int dS = *(int *)arg;
    int nb_recv;

    while(1) {

        // Read message

        nb_recv = recv(dS, msg, buffer_size, 0);
        if (nb_recv == -1) {
            perror("Erreur lors de la reception du message");
            close(dS);
            exit(EXIT_FAILURE);
        } else if (nb_recv == 0) {
            // Connection closed by client or server
            break;
        }
            
        afficher(34, "%s\n", msg);   

    }

    pthread_exit(0);
}


void *writeMessage(void *arg) {
    // Fonction qui envoie les messages au serveur
    int dS = *(int *)arg;
    int nb_send;
    char msg_pseudo[max_length + pseudo_length];

    afficher(31, "", NULL);

    while(1) {

        do{
            fgets(input, max_length, stdin);
            char *pos = strchr(input, '\n');
            *pos = '\0';
            //Remonte le curseur d'une ligne
            printf("\033[1A");

            if (strlen(input) <= 0){ 
                afficher(31, "", NULL);}

        } while(strlen(input) <= 0);



        if (strcmp(input, "fin") == 0) {
            // envoie "pseudo" est parti
            strcpy(msg_pseudo, pseudo);
            strcat(msg_pseudo, " est parti");
        }
        else{
            // Concatenate pseudo and message
            strcpy(msg_pseudo, pseudo);
            strcat(msg_pseudo, ": ");
            strcat(msg_pseudo, input);
        }

        afficher(32, "%s\n", msg_pseudo);

        // Send message
        nb_send = send(dS, msg_pseudo, buffer_size, 0);
        if (nb_send == -1) {
            perror("Erreur lors de l'envoi du message");
            close(dS);
            exit(EXIT_FAILURE);
        } else if (nb_send == 0) {
            // Connection closed by remote host
            afficher(31, "Le serveur a ferme la connexion\n", NULL);
            close(dS);
            break;
        }

        // Check if the message is "fin"
        // If it is, close the socket and exit
        if (strcmp(input, "fin") == 0) {
            afficher(31, "Vous mettez fin a la discussion\n", NULL);
            nb_send = send(dS, input, buffer_size, 0);
            if (nb_send == -1) {
                perror("Erreur lors de l'envoi du message");
                close(dS);
                exit(EXIT_FAILURE);
            } else if (nb_send == 0) {
                // Connection closed by remote host
                afficher(31, "Le serveur a ferme la connexion\n", NULL);
            }
            //Ferme la socket
            close(dS);
            break;
        }

    }

    pthread_exit(0);
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

    printf("\033[2J"); //Clear the screen
    
    // Demande le pseudo
    printf("Entrez votre pseudo (max %d caracteres) : ", pseudo_length - 1);
    do {
        fgets(pseudo, pseudo_length, stdin);
        if (strlen(pseudo) >= pseudo_length - 1) {
            printf("Pseudo trop long, veuillez en saisir un autre (max %d caracteres) : ", pseudo_length - 1);
            //vide le buffer de fgets
            int c;
            while ((c = getchar()) != '\n' && c != EOF){};

        } 
        // minimum 3 caractères
        else if (strlen(pseudo) < 3) {
            printf("Pseudo trop court, veuillez en saisir un autre (max %d caracteres) : ", pseudo_length - 1);
        }
    } while (strlen(pseudo) >= pseudo_length - 1 || strlen(pseudo) < 3);
    char *pos = strchr(pseudo, '\n');
    *pos = '\0';



    printf("Debut programme client\n");

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

    int nb_send;

    // Envoie "Pseudo" se connecte
    strcpy(msg, pseudo);
    strcat(msg, " se connecte");
    nb_send = send(dS, msg, buffer_size, 0);
    if (nb_send == -1) {
        perror("Erreur lors de l'envoi du message");
        close(dS);
        exit(EXIT_FAILURE);
    } else if (nb_send == 0) {
        // Connection closed by remote host
        afficher(31, "Le serveur a ferme la connexion\n", NULL);
        close(dS);
        exit(EXIT_FAILURE);
    }


    // Gestion du signal SIGINT (Ctrl+C)
    signal(SIGINT, handle_sigint);

    // Initialisation des threads
    pthread_t readThread;
    pthread_t writeThread;

    printf("\033[2J"); //Clear the screen
    printf("Bienvenue sur la messagerie instantanee !\n");
    printf("Vous etes connecte au serveur %s:%s en tant que %s.\n\n", argv[1], argv[2], pseudo);


    // Lancement du thread de lecture
    if (pthread_create(&readThread, NULL, readMessage, &dS) != 0) {
        perror("Erreur lors de la creation du thread de lecture");
        close(dS);
        exit(EXIT_FAILURE);
    }
    // Lancement du thread d'écriture
    if (pthread_create(&writeThread, NULL, writeMessage, &dS) != 0) {
        perror("Erreur lors de la creation du thread d'ecriture");
        close(dS);
        exit(EXIT_FAILURE);
    }




    // Attente de la fin des threads
    if (pthread_join(readThread, NULL) != 0) {
        perror("Erreur lors de la fermeture du thread de lecture");
        close(dS);
        exit(EXIT_FAILURE);
    }
    if (pthread_join(writeThread, NULL) != 0) {
        perror("Erreur lors de la fermeture du thread d'ecriture");
        close(dS);
        exit(EXIT_FAILURE);
    }

    //Efface les 2 dernières lignes
    printf("\033[2K\033[1A\033[2K\r");

    return EXIT_SUCCESS;
}