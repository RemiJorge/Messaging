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
// to talk with other clients
// It uses the TCP protocol
// It takes two arguments, the server ip and the server port
// If at some point the client sends "fin",
// the server will disconnect the client.
// The client needs to provide a username before connecting to the server

// You can use gcc to compile this program:
// gcc -o client client.c

// Use : ./client <server_ip> <server_port>

#define PSEUDO_LENGTH 10 // taille maximal du pseudo
#define CMD_LENGTH 10 // taille maximal de la commande
#define MSG_LENGTH 980 // taille maximal du message
#define MAX_LENGTH 980 // taille maximal du message
#define BUFFER_SIZE PSEUDO_LENGTH + CMD_LENGTH + MSG_LENGTH // taille maximal du message envoyé au serveur
char pseudo[PSEUDO_LENGTH]; // pseudo de l'utilisateur
char *array_color [7] = {"\033[31m", "\033[32m", "\033[33m", "\033[34m", "\033[35m", "\033[36m", "\033[37m"};
char *color; // couleur attribuée à l'utilisateur

// Struct for the messages
typedef struct Message Message;
struct Message {
    // The command
    char cmd[CMD_LENGTH];
    // If the server is receiving the message:
        // If the commend is "dm", the username of the client to send the message to
        // If the command is "who", username is empty
        // If the command is "fin", username is empty
        // If the command is "list", username is empty
    // If the server is sending the message:
        // If the command is "dm", username is who sent the message
        // If the command is "who", username is Server
        // If the command is "list", username is Server
    char username[PSEUDO_LENGTH];
    // The message
    // If the server is receiving the message:
        // If the command is "fin", message is empty
        // If the commande is "who", message is empty
        // If the command is "list", message is empty
        // If the command is "dm", message is the message to send
    // If the server is sending the message:
        // If the command is "who", message is username the client who asked
        // If the command is "list", message is the list of the connected clients
        // If the command is "dm", message is the message sent
    char message[MSG_LENGTH];
};



void *afficher(int color, char *msg, void *args){
    /*  Fonction formatant l'affichage 
        color : couleur du texte
        msg : message à afficher
        args : arguments du message
    */

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
    printf("---------- Entrez un message (max %d caracteres) -----------\n", MAX_LENGTH - 1);
    //Met le texte en gras
    printf("\033[1m");
    printf("Saisie : ");
    //Remet le texte en normal
    printf("\033[0m");
    //Flush le buffer de stdout
    fflush(stdout); 
    return NULL;
}


void print_message(Message *output){
    char msg[BUFFER_SIZE];
    sprintf(msg, "%s%s%s%s%s", output->username, " : ", output->message, "\n", "\0");
    afficher(32, msg, NULL);
}



void *readMessage(void *arg) {
    /*  Fonction qui lit les messages envoyés par le serveur
        arg : socket du serveur
    */
    int dS = *(int *)arg; // socket du serveur

    Message *response = malloc(sizeof(Message));
    int nb_recv; // nombre de caractères reçus

    // Fin de boucle si le serveur ferme la connexion ou si le client envoie "fin"
    while(1) {

        // Recoit le message des autres clients

        nb_recv = recv(dS, response, BUFFER_SIZE, 0);
        if (nb_recv == -1) {
            perror("Erreur lors de la reception du message");
            close(dS);
            exit(EXIT_FAILURE);
        } else if (nb_recv == 0) {
            // Connection closed by client or server
            break;
        }
            
        print_message(response);  
        
    }

    free(response);
    pthread_exit(0);
}



void *writeMessage(void *arg) {
    /*  Fonction qui envoie les messages au serveur
        arg : socket du serveur
    */ 

    int dS = *(int *)arg;
    int nb_send;

    Message *request = malloc(sizeof(Message));
    char input[MSG_LENGTH]; // message saisi par l'utilisateur

    afficher(31, "", NULL);

    while(1) {

        do{
            fgets(input, MSG_LENGTH, stdin);
            char *pos = strchr(input, '\n');
            if (pos != NULL){        
                *pos = '\0';
            }
            //Remonte le curseur d'une ligne
            printf("\033[1A");

            if (strlen(input) <= 0){ 
                afficher(31, "", NULL);}

        } while(strlen(input) <= 0);

        // Si l'input commence par "/man", affiche l'aide stockée dans le fichier "./manuel.txt"
        if (strcmp(input,"/man") == 0){
            FILE *fichier = NULL;
            fichier = fopen("../src/manuel.txt", "r");
            if (fichier != NULL){
                char ligne[100];
                while (fgets(ligne, 100, fichier) != NULL){
                    afficher(32, "%s", ligne);
                }
                fclose(fichier);
            }
            else{
                afficher(31, "Erreur lors de l'ouverture du fichier manuel.txt\n", NULL);
            }
            continue;
        }

        // Formatage du message
        strcpy(request->cmd, "");
        strcpy(request->username, pseudo);
        strcpy(request->message, input);

        // Si l'input est "/fin", ferme la connexion avec le serveur
        if (strcmp(input, "/fin") == 0){
            strcpy(request->cmd, "fin");
        }


        if (strcmp(input, "/who") == 0){
            strcpy(request->cmd, "who");
        }

        if (strcmp(input, "/list") == 0){
            strcpy(request->cmd, "list");
        }

        // Si l'input est "/mp pseudo message", envoie un message privé au client "pseudo"
        // utiliser strtok pour séparer les arguments
        char *traitement = strtok(input, " ");
        if (strcmp(traitement, "/mp") == 0){
            strcpy(request->cmd, "dm");
            traitement = strtok(NULL, " ");
            if (traitement == NULL){
                afficher(31, "Erreur : veuillez entrer un pseudo\n  /mp <pseudo> <message>\n", NULL);
                continue;
            }
            strcpy(request->username, traitement);
            traitement = strtok(NULL, "\0");
            if (traitement == NULL){
                afficher(31, "Erreur : veuillez entrer un message\n  /mp <pseudo> <message>\n", NULL);
                continue;
            }
            strcpy(request->message, traitement);
        }


        

        // Envoie le message au serveur
        nb_send = send(dS, request, BUFFER_SIZE, 0);
        if (nb_send == -1) {
            perror("Erreur lors de l'envoi du message");
            close(dS);
            exit(EXIT_FAILURE);
        } else if (nb_send == 0) {
            // Connection fermée par le client ou le serveur
            afficher(31, "Le serveur a ferme la connexion\n", NULL);
            close(dS);
            break;
        }

        // Si le client envoie "fin", on ferme la socket
        if (strcmp(input, "/fin") == 0) {
            afficher(31, "Vous mettez fin a la discussion\n", NULL);
            //Ferme la socket
            close(dS);
            break;
        }

        print_message(request);

    }

    free(request);
    pthread_exit(0);
}



void handle_sigint(int sig) {
    // Fonction qui gère le signal SIGINT (Ctrl+C)
    // On a fait le choix de desactiver le Ctrl+C pour eviter que le client ne quitte la discussion sans le vouloir
    // On aurait pu aussi envoyer un message au serveur pour le prevenir que le client quitte la discussion
    afficher(31, "Pour quitter, veuillez saisir le mot 'fin'.\n", NULL);
}




int main(int argc, char *argv[]) {

    if (argc != 3) {
        printf("Error: You must provide exactly 2 arguments.\n\
                Usage: %s <server_ip> <server_port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    printf("\033[2J"); //Clear the screen
    

    // Choisi une couleur random pour le client parmis les 7 couleurs disponibles dans array_color et stocke le pointeur dans color
    srand(time(NULL));
    color = array_color[rand() % 7];


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

    int pseudo_valide = 0;
    int nb_send;
    char nb_recv;
    Message *request = malloc(sizeof(Message));

    // Demande le pseudo
    printf("Entrez votre pseudo (max %d caracteres) : ", PSEUDO_LENGTH - 1);
    do{ 
        do {
            fgets(pseudo, PSEUDO_LENGTH, stdin);
            char *pos = strchr(pseudo, '\n');
            if (pos != NULL){        
                *pos = '\0';
            }
            // vider le buffer lorque le pseudo est trop long
            if (strlen(pseudo) >= PSEUDO_LENGTH - 1) { // c'est le cas d'egalite qui est important
                //vide le buffer de fgets
                int c;
                while ((c = getchar()) != '\n' && c != EOF){};
            }

            // minimum 3 caractères
            if (strlen(pseudo) < 3) {
                printf("Pseudo trop court, veuillez en saisir un autre (max %d caracteres) : ", PSEUDO_LENGTH - 1);
                strcpy(pseudo, "");
            }
        } while (strlen(pseudo) < 3 || strlen(pseudo) >= PSEUDO_LENGTH - 1);
        
        printf("Vous avez choisi le pseudo : %s\n", pseudo);

        // Preparation du request
        strcpy(request -> cmd, "");
        strcpy(request -> username, pseudo);
        strcpy(request -> message, "");

        printf("Envoie du pseudo au serveur\n");
        // Envoie le request au serveur
        nb_send = send(dS, request, BUFFER_SIZE, 0);
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

        // Reception de la reponse du serveur
        nb_recv = recv(dS, request, BUFFER_SIZE, 0);
        if (nb_recv == -1) {
            perror("Erreur lors de la reception du message");
            close(dS);
            exit(EXIT_FAILURE);
        } else if (nb_recv == 0) {
            // Connection closed by remote host
            afficher(31, "Le serveur a ferme la connexion\n", NULL);
            close(dS);
            exit(EXIT_FAILURE);
        }

        // Si le pseudo est valide
        if (strcmp(request -> message, "true") == 0) {
            pseudo_valide = 1;
        } else {
            printf("Pseudo non disponible, veuillez en saisir un autre (max %d caracteres) : ", PSEUDO_LENGTH - 1);
            strcpy(pseudo, "");
        }

    } while (pseudo_valide == 0);



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