#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <time.h>
#include <termios.h>
#include <dirent.h>
#include <semaphore.h>


// DOCUMENTATION
// This program is will execute when a client joins a channel
// Please read the documentation for more information   


/*******************************************
            VARIABLES GLOBALES
********************************************/

#define PSEUDO_LENGTH 10 // taille maximal du pseudo
#define CMD_LENGTH 10 // taille maximal de la commande
#define MSG_LENGTH 960 // taille maximal du message
#define COLOR_LENGTH 10 // taille de la couleur
#define CHANNEL_SIZE 10 // Size of the channel name
#define BUFFER_SIZE PSEUDO_LENGTH + PSEUDO_LENGTH + CMD_LENGTH + MSG_LENGTH + COLOR_LENGTH + CHANNEL_SIZE// taille maximal du message envoyé au serveur
#define FILES_DIRECTORY "../src/client_files/" // répertoire courant
char pseudo[PSEUDO_LENGTH]; // pseudo de l'utilisateur
char *array_color [11] = {"\033[32m", "\033[33m", "\033[34m", "\033[35m", "\033[36m", "\033[91m", "\033[92m", "\033[93m", "\033[94m", "\033[95m", "\033[96m"};
char color[COLOR_LENGTH]; // couleur attribuée à l'utilisateur
char *server_ip; // ip du serveur
int server_port; // port du serveur
char channel_nom[CHANNEL_SIZE]; // nom du salon

// The thread ids of the read and write threads
pthread_t readThread;
pthread_t writeThread;





// Struct for the messages
typedef struct Message Message;
struct Message {
    // The command
    // Possible commands : "dm", "who", "fin", "list", "upload", "download"
    char cmd[CMD_LENGTH];
    // The username of the client who will receive the message
    // It can be Server if the message is sent from the server
    char from[PSEUDO_LENGTH];
    // The username of the client who sent the message
    // It can be Server if the message is sent to the server
    char to[PSEUDO_LENGTH];
    // The channel name
    char channel[CHANNEL_SIZE];
    // The message
    char message[MSG_LENGTH];
    // The color of the message
    char color[COLOR_LENGTH];
};



/*******************************************
            FONCTIONS UTILITAIRES
********************************************/


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
    printf("---------- Entrez un message (max %d caracteres) -----------\n", MSG_LENGTH - 1);
    //Met le texte en gras
    printf("\033[1m");
    printf("%s : ", channel_nom);
    //Remet le texte en normal
    printf("\033[0m");
    //Flush le buffer de stdout
    fflush(stdout);
    return NULL;
}



void getCurrentTime(char* timeString, int maxBufferSize) {
    time_t currentTime;
    struct tm* timeInfo;

    // Get the current time
    time(&currentTime);

    // Convert the current time to the local time
    timeInfo = localtime(&currentTime);

    // Format the time as a string
    strftime(timeString, maxBufferSize, "%H:%M | ", timeInfo);
}




void print_message(Message *output){
    char msg[BUFFER_SIZE + 50]; // car "mp de " fait 6 caracteres de plus
    char color_message[COLOR_LENGTH];
    char timeString[20];
    strcpy(color_message, output->color);
    strcpy(msg, color_message);
    // on met la date au debut du message
    getCurrentTime(timeString, 20);
    strcat(msg, timeString);
    if (strcmp(output->cmd, "dm") == 0){
        strcat(msg, "mp de ");
    }
    strcat(msg, output->from);
    strcat(msg, " : ");
    strcat(msg, "\033[0m");

    // Si le message contient @<pseudo> on met la mention en surbrillance
    char* start = output->message;
    // Tant qu'on a pas atteint la fin du message
    // On cherche une mention parmis toutes les mentions
    while (*start != '\0') {
        char* pos = strstr(start, "@");
        if (pos == NULL) {
            strcat(msg, start);
            break;
        }
        // Copie la partie du message avant la mention
        // pos - start = longueur de la partie du message avant la mention
        strncat(msg, start, pos - start);
        start = pos + 1;
        // Récupère le pseudo de la mention
        char* pseudo_start = start;
        while (*start != '\0' && *start != ' ') {
            start++;
        }
        char mention_pseudo[PSEUDO_LENGTH];
        // Copie le pseudo de la mention
        strncpy(mention_pseudo, pseudo_start, start - pseudo_start);
        mention_pseudo[start - pseudo_start] = '\0';
        // Ajoute la surbrillance si le pseudo correspond
        if (strcmp(mention_pseudo, pseudo) == 0 || strcmp(mention_pseudo, "everyone") == 0) {
            strcat(msg, "\033[42m@");
            strncat(msg, pseudo_start, start - pseudo_start);
            strcat(msg, "\033[0m");
        } else {
            strcat(msg, "@");
            strncat(msg, pseudo_start, start - pseudo_start);
        }
    }

    strcat(msg, "\n\0");

    afficher(32, msg, NULL);
}

void print_dm_envoye(Message *output){
    char msg[BUFFER_SIZE + 20]; // car "mp envoye a " fait 13 caracteres de plus
    char timeString[20];
    getCurrentTime(timeString, 20);
    sprintf(msg, "%s%s%s%s%s%s%s%s", color, timeString, "mp envoye a ", output->to, " : ", "\033[0m", output->message, "\n\0");
    afficher(32, msg, NULL);
}



/*******************************************
            FONCTIONS DE THREADS
********************************************/

/************** LECTURE ****************/

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
            afficher(31, "Le serveur a ferme la connexion\n", NULL);
            pthread_cancel(writeThread);
            close(dS);
            break;
        }

        // Si la cmd est "end" le salon a ete supprime
        if (strcmp(response->cmd, "end") == 0){
            afficher(31, "Le salon a ete supprime\n", NULL);
            pthread_cancel(writeThread);
            sleep(2);
            close(dS);
            break;
        }

        // Si la cmd est "exit" le client a voulu quitter le salon
        if (strcmp(response->cmd, "exit") == 0){
            afficher(31, "Vous avez quitte le salon\n", NULL);
            sleep(2);
            close(dS);
            break;
        }

        // Si la cmd est "exitm" le client quitte le salon depuis le menu
        if (strcmp(response->cmd, "exitm") == 0){
            afficher(31, "Vous avez quitte le salon depuis le menu\n", NULL);
            pthread_cancel(writeThread);
            sleep(2);
            close(dS);
            break;
        }

        print_message(response);
    }

    free(response);
    pthread_exit(0);
}


/************** ECRITURE ****************/

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
        strcpy(request->from, pseudo);
        strcpy(request->to, "all");
        strcpy(request->channel, "global");
        strcpy(request->message, input);
        strcpy(request->color, color);


        // Si l'input est "/exit", ferme le salon et déconnecte le client
        if (strcmp(input, "/exit") == 0){
            strcpy(request->cmd, "exit");
            print_message(request);
            // On envoit le message au serveur
            nb_send = send(dS, request, BUFFER_SIZE, 0);
            if (nb_send == -1) {
                perror("Erreur lors de l'envoi du message");
                close(dS);
                exit(EXIT_FAILURE);
            } else if (nb_send == 0) {
                // Connection fermée par le client ou le serveur
                afficher(31, "Le serveur a ferme la connexion\n", NULL);
                pthread_cancel(readThread);
                close(dS);
                break;
            }
            // On ferme le socket
            close(dS);
            break;
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
            strcpy(request->to, traitement);
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
            pthread_cancel(readThread);
            close(dS);
            break;
        }

        if (strcmp(request->cmd, "dm") == 0){
            print_dm_envoye(request);
        } else {
            print_message(request);
        }

    }

    free(request);
    pthread_exit(0);
}




/*******************************************
            GESTIONS DES SIGNAUX
********************************************/

void handle_sigint(int sig) {
    // Fonction qui gère le signal SIGINT (Ctrl+C)
    // On a fait le choix de desactiver le Ctrl+C pour eviter que le client ne quitte la discussion sans le vouloir
    // On aurait pu aussi envoyer un message au serveur pour le prevenir que le client quitte la discussion
    afficher(31, "Pour quitter, veuillez saisir '/exit'.\n", NULL);
}


/*******************************************
                MAIN
********************************************/


int main(int argc, char *argv[]) {
    // prend en argument le port du serveur, le pseudo et la couleur du client

    if (argc != 5) {
        printf("Error: You must provide exactly 3 arguments.\n\
                Usage: ./client <server_port> <pseudo> <color>\n");
        exit(EXIT_FAILURE);
    }
    server_ip = "127.0.0.1";
    server_port = atoi(argv[1]);
    strcpy(pseudo, argv[2]);
    strcpy(color, argv[3]);
    strcpy(channel_nom, argv[4]);



    system("clear"); // Efface l'écran


    printf("Debut programme client\n");

    int dS = socket(PF_INET, SOCK_STREAM, 0);
    if (dS == -1) {
        perror("Erreur creation socket");
        exit(EXIT_FAILURE);
    }

    printf("Socket Créé\n");

    struct sockaddr_in aS;

    aS.sin_family = AF_INET;
    int result = inet_pton(AF_INET, server_ip, &(aS.sin_addr));
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
    aS.sin_port = htons(server_port);
    socklen_t lgA = sizeof(struct sockaddr_in) ;
    if (connect(dS, (struct sockaddr *) &aS, lgA) == -1) {
        perror("Erreur connect client");
        exit(EXIT_FAILURE);
    }

    printf("Socket Connecté\n");

    // Gestion du signal SIGINT (Ctrl+C)
    signal(SIGINT, handle_sigint);


    system("clear"); // Efface l'écran
    printf("Bienvenue sur le salon %s !\n", channel_nom);
    printf("Vous etes connecte au serveur %s:%s en tant que %s.\n\n", server_ip, argv[1], pseudo);


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
    if (pthread_join(writeThread, NULL) != 0) {
        perror("Erreur lors de la fermeture du thread d'ecriture");
        close(dS);
        exit(EXIT_FAILURE);
    }
    if (pthread_join(readThread, NULL) != 0) {
        perror("Erreur lors de la fermeture du thread de lecture");
        close(dS);
        exit(EXIT_FAILURE);
    }


    //Efface les 2 dernières lignes
    printf("\033[2K\033[1A\033[2K\r");

    return EXIT_SUCCESS;
}