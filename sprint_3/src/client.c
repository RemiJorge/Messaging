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

// DOCUMENTATION
// This program acts as a client which connects to a server
// to talk with other clients
// It uses the TCP protocol
// It takes two arguments, the server ip and the server port
// The client needs to provide a unique username to chat with other clients

// Please read the README.md file for more information
// including the different commands that can be used

// You can use gcc to compile this program:
// gcc -o client client.c

// Use : ./client <server_ip> <server_port>
//
/*******************************************
            VARIABLES GLOBALES
********************************************/

#define PSEUDO_LENGTH 10 // taille maximal du pseudo
#define CMD_LENGTH 10 // taille maximal de la commande
#define MSG_LENGTH 960 // taille maximal du message
#define COLOR_LENGTH 10 // taille de la couleur
#define BUFFER_SIZE PSEUDO_LENGTH + PSEUDO_LENGTH + CMD_LENGTH + MSG_LENGTH + COLOR_LENGTH// taille maximal du message envoyé au serveur
#define FILES_DIRECTORY "../src/client_files/" // répertoire courant
char pseudo[PSEUDO_LENGTH]; // pseudo de l'utilisateur
char *array_color [11] = {"\033[32m", "\033[33m", "\033[34m", "\033[35m", "\033[36m", "\033[91m", "\033[92m", "\033[93m", "\033[94m", "\033[95m", "\033[96m"};
char *color; // couleur attribuée à l'utilisateur
char *server_ip; // ip du serveur
int server_port; // port du serveur  
int num_files; // nombre de fichiers dans le menu, 0 signifie que le menu n'est pas ouvert
int index_cursor = 0; // index_cursor du fichier sélectionné dans le menu de téléchargement

void *afficher(int color, char *msg, void *args);


// Struct for the messages
typedef struct Message Message;
struct Message {
    // The command
    // Possible commands : "dm", "who", "fin", "list", "upload", "download", "endu", "endd"
    char cmd[CMD_LENGTH];
    // If the server is receiving the message:
        // If the commend is "dm", the username of the client to send the message to
        // If the command is "who", username is empty
        // If the command is "fin", username is empty
        // If the command is "list", username is empty
        // If the client is trying to connect, username is the username of the client
    // If the server is sending the message:
        // If the command is "dm", username is who sent the message
        // If the command is "who", username is Server
        // If the command is "list", username is Server
    char from[PSEUDO_LENGTH];
    char to[PSEUDO_LENGTH];
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
    // The color of the message
    char color[COLOR_LENGTH];
};



/*******************************************
            FONCTIONS UTILITAIRES
********************************************/

// Fonction pour désactiver le mode canonique du terminal
void disableCanonicalMode() {
    struct termios term;
    tcgetattr(STDIN_FILENO, &term);
    term.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &term);
}

// Fonction pour activer le mode canonique du terminal
void enableCanonicalMode() {
    struct termios term;
    tcgetattr(STDIN_FILENO, &term);
    term.c_lflag |= ICANON | ECHO;
    tcsetattr(STDIN_FILENO, TCSANOW, &term);
}

// Fonction pour afficher les fichiers du répertoire de téléchargement
int display_files() {
    //renvoie le nombre de fichiers dans le répertoire
    DIR *directory;
    struct dirent *file;

    directory = opendir(FILES_DIRECTORY);
    
    printf("\033[35m---------- Choisissez un fichier à envoyer ----------\n");

    int nb_file = 1;
    printf("   retour au tchat\n");
    if (directory) {
        while ((file = readdir(directory)) != NULL) {
            if (strcmp(file->d_name, ".") != 0 && strcmp(file->d_name, "..") != 0) {
                printf("   %s\n", file->d_name);
                nb_file++;
            }
        }
        closedir(directory);
    }

    printf("\033[0m");

    return nb_file;
}

void display_cursor() {
    // Affiche le curseur à la bonne position
    // Sauvegarde la position du curseur
    printf("\033[s");
    // Remonte le curseur de num_files-index_cursor lignes
    printf("\033[%dA", num_files - index_cursor);
    printf("-> ");
    fflush(stdout);
    // Restaure la position du curseur
    printf("\033[u");
}

void clear_cursor() {
    // Efface le curseur à la bonne position
    // Sauvegarde la position du curseur
    printf("\033[s");
    // Remonte le curseur de num_files-index_cursor lignes
    printf("\033[%dA", num_files - index_cursor);
    printf("   ");
    fflush(stdout);
    // Restaure la position du curseur
    printf("\033[u");
}


// Fonction pour obtenir le fichier choisi par l'utilisateur avec les flèches du clavier
char* get_file() {
    disableCanonicalMode();

    //Efface les deux dernières lignes
    printf("\033[2K\r\033[1A\033[2K\r");

    DIR *directory;
    struct dirent *file;

    directory = opendir(FILES_DIRECTORY); // Remplacez "repertoire_de_telechargement" par le chemin du répertoire de téléchargement

    num_files = display_files();

    int c;
    index_cursor = 0;
    do {

        display_cursor();

        c = getchar();
        if (c == 27) { // Vérifie si une séquence d'échappement a été détectée
            getchar(); // Ignore le caractère '['
            clear_cursor(index_cursor, num_files);
            switch (getchar()) {
                case 'A': // Flèche vers le haut
                    index_cursor = (index_cursor - 1 + num_files) % num_files;
                    break;
                case 'B': // Flèche vers le bas
                    index_cursor = (index_cursor + 1) % num_files;
                    break;
                   }
        }
    } while (c != '\n'); // Sort de la boucle lorsque l'utilisateur appuie sur la touche Entrée

    enableCanonicalMode();

    char* filename = NULL;
    
    if (index_cursor >0) {

        // Récupère le nom du fichier sélectionné
        int currentIndex = 1;

        directory = opendir(FILES_DIRECTORY); // Remplacez "repertoire_de_telechargement" par le chemin du répertoire de téléchargement

        if (directory) {
            while ((file = readdir(directory)) != NULL) {
                if (strcmp(file->d_name, ".") != 0 && strcmp(file->d_name, "..") != 0) {
                    if (currentIndex == index_cursor) {
                        filename = strdup(file->d_name);
                        break;
                    }
                    currentIndex++;
                }
            }
            closedir(directory);
        }
    
    }


    //clear the number of files
    for (int i = 0; i <= num_files; i++) {
        printf("\033[1A\033[2K\r");
    }
    index_cursor = 0;
    num_files = 0;
    afficher(31, "", NULL);

    return filename;

}


char * get_file_download(char *files) {
    // Fonction pour obtenir le fichier choisi par l'utilisateur avec les flèches du clavier
    disableCanonicalMode();

    //Efface les deux dernières lignes
    printf("\033[2K\r\033[1A\033[2K\r");
    printf("\033[35m---------- Choisissez un fichier à recevoir ----------\n");
    printf("   retour au tchat\n");

    char *file = strtok(files, "/");
    char *files_array[100];
    int i = 0;
    while (file != NULL) {
        files_array[i] = file;
        printf("   %s\n", files_array[i]);
        file = strtok(NULL, "/");
        i++;
    }
    
    num_files = i + 1;
    index_cursor = 0;
    int c;
    do {

        display_cursor();

        c = getchar();
        if (c == 27) { // Vérifie si une séquence d'échappement a été détectée
            getchar(); // Ignore le caractère '['
            clear_cursor(index_cursor, num_files);
            switch (getchar()) {
                case 'A': // Flèche vers le haut
                    index_cursor = (index_cursor - 1 + num_files) % num_files;
                    break;
                case 'B': // Flèche vers le bas
                    index_cursor = (index_cursor + 1) % num_files;
                    break;
                   }
        }
    } while (c != '\n'); // Sort de la boucle lorsque l'utilisateur appuie sur la touche Entrée

    enableCanonicalMode();

    char* filename = NULL;
    
    if (index_cursor >0) {

        // Récupère le nom du fichier sélectionné
        if (num_files > 0) {
            filename = files_array[index_cursor - 1];
        }

    }

    //clear the number of files
    for (int i = 0; i <= num_files; i++) {
        printf("\033[1A\033[2K\r");
    }
    num_files = 0;
    index_cursor = 0;
    afficher(31, "", NULL);

    return filename;

}


void *afficher(int color, char *msg, void *args){
    /*  Fonction formatant l'affichage
        color : couleur du texte
        msg : message à afficher
        args : arguments du message
    */
    // TODO affcihage lorsque on est dans le menu download
    if (num_files > 0){
        //clear the number of files
        for (int i = 0; i <= num_files; i++) {
            printf("\033[1A\033[2K\r");
        }
        //Change la couleur du texte
        printf("\033[%dm", color);
        //Affiche le message
        printf(msg, args);
        printf("\n\033[0m");

        display_files();
        display_cursor();

        fflush(stdout);
        return NULL;
    }

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
    printf("Saisie : ");
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
            THREADS DES FICHIERS
********************************************/

long get_file_size(FILE *file) {
    long size;
    fseek(file, 0, SEEK_END);  // Déplace le curseur à la fin du fichier
    size = ftell(file);        // Récupère la position actuelle du curseur (qui est la taille du fichier)
    rewind(file);              // Remet le curseur au début du fichier
    return size;
}

/***************** UPLOAD ******************/


void *upload_file(void* param){
    // Fonction qui envoie un fichier au serveur (thread)
    // On utilise un thread pour pouvoir envoyer un message au serveur pendant l'envoi du fichier

    struct upload_param {
        char *filename;
        FILE *file;
    };
    struct upload_param param_upload = *(struct upload_param*) param;
    char *filename = param_upload.filename;

    FILE *fichier = (FILE*) param_upload.file;
    if (fichier == NULL){
        afficher(31, "Erreur lors de l'ouverture du fichier\n", NULL);
        pthread_exit(0);
    }



    long size_file = 0;
    size_file = get_file_size(fichier);
    //printf("Taille du fichier : %ld\n", size_file);

    Message *request = malloc(sizeof(Message));

    int dS = socket(AF_INET, SOCK_STREAM, 0);
    if (dS == -1) {
        perror("Erreur lors de la creation de la socket");
        exit(EXIT_FAILURE);
    }

    //printf("Socket Créé\n");
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

    aS.sin_port = htons(server_port + 1); // +1 pour le port d'upload du fichier
    socklen_t lgA = sizeof(struct sockaddr_in) ;
    if (connect(dS, (struct sockaddr *) &aS, lgA) == -1) {
        perror("Erreur connect client");
        exit(EXIT_FAILURE);
    }

    //printf("Socket Connecté\n");
    // Formatage du message
    strcpy(request->cmd, "upload");
    strcpy(request->from, pseudo);
    strcpy(request->to, "server");
    strcpy(request->message, filename);
    strcpy(request->color, color);

    // Envoie le nom du fichier au serveur
    //printf("Envoie du nom du fichier au serveur\n");
    int nb_send = send(dS, request, BUFFER_SIZE, 0);
    if (nb_send == -1) {
        perror("Erreur lors de l'envoi du message");
        close(dS);
        exit(EXIT_FAILURE);
    } else if (nb_send == 0) {
        // Connection fermée par le client ou le serveur
        afficher(31, "Le serveur a ferme la connexion\n", NULL);
        close(dS);
        exit(EXIT_FAILURE);
    }


    // Envoie la taille du fichier au serveur
    //printf("Envoie de la taille du fichier au serveur\n");
    nb_send = send(dS, &size_file, sizeof(long), 0);
    if (nb_send == -1) {
        perror("Erreur lors de l'envoi du message");
        close(dS);
        exit(EXIT_FAILURE);
    } else if (nb_send == 0) {
        // Connection fermée par le client ou le serveur
        afficher(31, "Le serveur a ferme la connexion\n", NULL);
        close(dS);
        exit(EXIT_FAILURE);
    }


    char buffer[BUFFER_SIZE];
    int nb_read_total = 0;
    int nb_read = 0;
    //printf("Envoie du fichier au serveur\n");
    // Envoie le fichier au serveur
    while(nb_read_total < size_file){
        nb_read = fread(buffer, 1, BUFFER_SIZE, fichier);
        nb_read_total += nb_read;
        // ajouter /0 a la fin du buffer si le fichier est plus petit que BUFFER_SIZE
        if (nb_read < BUFFER_SIZE){ // inutile mais pour un code robuste
            buffer[nb_read] = '\0';
        }
        //printf("Taille du fichier : %ld, Taille lu: %d\n, Taille lu total:%d\n", size_file, nb_read, nb_read_total);
        if (nb_read < BUFFER_SIZE){
            nb_send = send(dS, buffer, nb_read, 0);
        }else{
            nb_send = send(dS, buffer, BUFFER_SIZE, 0);
        }
        //printf("Taille envoye: %d\n", nb_send);
        //printf("Message envoye: %s\n", buffer);
        if (nb_send == -1) {
            perror("Erreur lors de l'envoi du message");
            close(dS);
            exit(EXIT_FAILURE);
        } else if (nb_send == 0) {
            // Connection fermée par le client ou le serveur
            afficher(31, "Le serveur a ferme la connexion\n", NULL);
            close(dS);
            exit(EXIT_FAILURE);
        }
        bzero(buffer, BUFFER_SIZE);
    }

    char msg[150];
    sprintf(msg, "Fichier envoye : %s (taille : %d/%ld)\n", filename, nb_read_total, size_file);

    afficher(32, msg, NULL);
    //printf("Fermeture de la socket\n");
    fclose(fichier);
    close(dS);
    //printf("Fichier fermé\n");
    free(request);
    //printf("Fichier envoyé\n");
    pthread_exit(0);
}


/***************** DOWNLOAD ******************/

void * download_file(void* param){
    // Fonction qui télécharge un fichier du serveur (thread)
    // Prend en argument le nom du fichier à télécharger et la socket du serveur
    // On utilise un thread pour pouvoir envoyer un message au serveur pendant le téléchargement du fichier
    struct download_param {
        char *filename;
        int *dS;
    };
    struct download_param param_download = *(struct download_param*) param;

    int dS = *(param_download.dS);

    Message buffer[BUFFER_SIZE];
    char *filename = param_download.filename;

    if (filename == NULL){ // retour au tchat
        close(dS);
        pthread_exit(0);
    }

    // On envoit le nom du fichier au serveur
    strcpy(buffer->message, filename);
    int nb_recv = send(dS, buffer, BUFFER_SIZE, 0);
    if (nb_recv == -1) {
        perror("Erreur lors de la reception du message");
        close(dS);
        exit(EXIT_FAILURE);
    } else if (nb_recv == 0) {
        // Connection fermée par le client ou le serveur
        afficher(31, "Le serveur a ferme la connexion\n", NULL);
        close(dS);
        exit(EXIT_FAILURE);
    }
    
    // on recoit la taille du fichier selectionner
    long file_size;
    nb_recv = recv(dS, &file_size, sizeof(long), 0);
    if (nb_recv == -1) {
        perror("Erreur lors de la reception du message");
        close(dS);
        exit(EXIT_FAILURE);
    } else if (nb_recv == 0) {
        // Connection fermée par le client ou le serveur
        afficher(31, "Le serveur a ferme la connexion\n", NULL);
        close(dS);
        exit(EXIT_FAILURE);
    }


    // puis on recoit le fichier
    FILE *fichier = NULL;
    char path[100];
    strcpy(path, FILES_DIRECTORY);
    strcat(path, filename);
    fichier = fopen(path, "w");
    if (fichier == NULL) {
        perror("Erreur lors de la creation du fichier");
        exit(EXIT_FAILURE);
    }

    char packet[BUFFER_SIZE];
    int nb_read_total = 0;
    while(1){
        nb_recv = recv(dS, packet, BUFFER_SIZE, 0);
        if (nb_recv == -1) {
            perror("Erreur lors de la reception");
            exit(EXIT_FAILURE);
        }
        // If ever a client disconnect while we are receiving the messages
        // or if he has sent everything and closed the socket
        if (nb_recv == 0) {
            //printf("Socket ferme : Le serveur a fini de upload\n");
            break;
        }
        nb_read_total += nb_recv;

        
        // If the nb_recv_total greater than the file_size, we stop the loop
        if (nb_read_total >= file_size + 1000) {
            break;
        }
                
        // We write in the file
        fwrite(packet, sizeof(char), nb_recv, fichier);
    }

    char msg[150];
    sprintf(msg, "Fichier reçu : %s (taille : %d/%ld)\n", filename, num_files, file_size);
    afficher(32, msg, NULL);
    // We close the file
    fclose(fichier);
    
    pthread_exit(0);
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
        strcpy(request->message, input);
        strcpy(request->color, color);


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
            strcpy(request->to, traitement);
            traitement = strtok(NULL, "\0");
            if (traitement == NULL){
                afficher(31, "Erreur : veuillez entrer un message\n  /mp <pseudo> <message>\n", NULL);
                continue;
            }
            strcpy(request->message, traitement);
        }

        // Si l'input est "/upload fichier" envoie "upload" au server
        // Si le fichier n'est pas spécifié, demande le fichier à uploader
        if (strcmp(traitement, "/upload") == 0){
            strcpy(request->cmd, "upload");
            traitement = strtok(NULL, " ");
            if (traitement == NULL){
                //Demander le fichier que l'on souhaite uploader
                traitement = get_file();
                if (traitement == NULL){
                    // L'option retour au tchat selectionner
                    continue;
                }
            }
            strcpy(request->message, "Envoie du fichier : ");
            strcat(request->message, traitement);

            //ajout du chemin du fichier
            char chemin[100];
            strcpy(chemin, FILES_DIRECTORY);
            strcat(chemin, traitement);


            if (access(chemin, F_OK) == -1){
                afficher(31, "Erreur : le fichier n'existe pas\n", NULL);
                continue;
            }

            // Ouverture du fichier
            FILE *fichier = NULL;
            fichier = fopen(chemin, "r");
            if (fichier == NULL){
                afficher(31, "Erreur lors de l'ouverture du fichier\n", NULL);
                continue;
            }

            pthread_t uploadThread;

            //parametre du thread : nom du fichier et fichier
            struct upload_param {
                char *filename;
                FILE *file;
            } param;
            param.filename = traitement;
            param.file = fichier;

            if (pthread_create(&uploadThread, NULL, upload_file, &param) != 0) {
                perror("Erreur lors de la creation du thread de lecture");
                close(dS);
                exit(EXIT_FAILURE);
            }
            
        }


        // Si l'input est "/download" envoie "download" au server
        if (strcmp(traitement, "/download") == 0){
            strcpy(request->cmd, "download");
            strcpy(request->message, "Demande de fichier au serveur");
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

            int dS_download = socket(AF_INET, SOCK_STREAM, 0);
            if (dS_download == -1) {
                perror("Erreur lors de la creation de la socket");
                exit(EXIT_FAILURE);
            }

            //printf("Socket Créé\n");
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

            aS.sin_port = htons(server_port + 2); // +2 pour le port du download du fichier
            socklen_t lgA = sizeof(struct sockaddr_in) ;
            if (connect(dS_download, (struct sockaddr *) &aS, lgA) == -1) {
                perror("Erreur connect client");
                exit(EXIT_FAILURE);
            }

            // reception des fichiers disponibles dans un struct message, les fichiers sont séparés par des "/"
            int nb_recv = recv(dS_download, request, BUFFER_SIZE, 0);
            if (nb_recv == -1) {
                perror("Erreur lors de la reception du message");
                close(dS);
                exit(EXIT_FAILURE);
            } else if (nb_recv == 0) {
                // Connection fermée par le client ou le serveur
                afficher(31, "Le serveur a ferme la connexion\n", NULL);
                close(dS);
                exit(EXIT_FAILURE);
            }

            char * filename = get_file_download(request->message);

            struct download_param {
                char *filename;
                int *dS;
            } param;

            param.filename = filename;
            param.dS = &dS_download;
            pthread_t downloadThread;

            if (pthread_create(&downloadThread, NULL, download_file, &param) != 0) {
                perror("Erreur lors de la creation du thread de lecture");
                close(dS);
                exit(EXIT_FAILURE);
            }
            continue;
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
    afficher(31, "Pour quitter, veuillez saisir le mot 'fin'.\n", NULL);
}


/*******************************************
                MAIN
********************************************/


int main(int argc, char *argv[]) {

    if (argc != 3) {
        printf("Error: You must provide exactly 2 arguments.\n\
                Usage: %s <server_ip> <server_port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    server_ip = argv[1];
    server_port = atoi(argv[2]);


    system("clear"); // Efface l'écran


    // Choisi une couleur random pour le client parmis les 11 couleurs disponibles dans array_color et stocke le pointeur dans color
    srand(time(NULL));
    color = array_color[rand() % 11];


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

    int pseudo_valide = 0;
    int nb_send;
    char nb_recv;
    Message *request = malloc(sizeof(Message));

    // Demande le pseudo
    printf("Entrez votre pseudo (max %d caracteres) : ", PSEUDO_LENGTH - 2);
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
        strcpy(request -> from, pseudo);
        strcpy(request -> to, "server");
        strcpy(request -> message, "");
        strcpy(request -> color, color);

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

    system("clear"); // Efface l'écran
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