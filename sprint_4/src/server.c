#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <time.h>
#include <dirent.h>
#include <signal.h>

// DOCUMENTATION
// This program acts as a server to relay messages between multiple clients
// It uses the TCP protocol
// It takes one argument, the port to use

// Please read the README.md file for more information
// including the different commands that can be used

// You can use gcc to compile this program:
// gcc -o serv server.c

// Use : ./serv <port>

/**************************************************
                    Constants
***************************************************/

// Maximum number of clients that can connect to the server
#define MAX_CLIENT 25
// Username size
#define USERNAME_SIZE 10
// Size of commands
#define CMD_SIZE 10
// Size of the message
#define MSG_SIZE 960
// Size of color
#define COLOR_SIZE 10
// Size of the channel name
#define CHANNEL_SIZE 10
// Buffer size for messages (this is the total size of the message)
#define BUFFER_SIZE USERNAME_SIZE + USERNAME_SIZE + CHANNEL_SIZE + CMD_SIZE + MSG_SIZE + COLOR_SIZE


/****************************************************
                List Type Def and Functions
*****************************************************/

// This list will hold elements of type string
// This list will be used to hold the names of the channels that each client is in

typedef struct List List;
typedef struct ElementList ElementList;

struct ElementList{
    char name[CHANNEL_SIZE];
    ElementList *next;
};


struct List{
    ElementList * premier;
    int count;
};


// Creates a new list
List * new_list(){
    List * l = malloc(sizeof(List));
    l->premier = NULL;
    l->count = 0;
    return l;
}

// Adds an element to the list
void add(List * l, char * name){
    ElementList * e = malloc(sizeof(ElementList));
    strcpy(e->name, name);
    e->next = NULL;
    if(l->premier == NULL){
        l->premier = e;
    }else{
        ElementList * current = l->premier;
        while(current->next != NULL){
            current = current->next;
        }
        current->next = e;
    }
    l->count++;
}

// Removes an element from the list
void remove_element(List * l, char * name){
    ElementList * current = l->premier;
    ElementList * previous = NULL;
    while(current != NULL){
        if(strcmp(current->name, name) == 0){
            if(previous == NULL){
                l->premier = current->next;
            }else{
                previous->next = current->next;
            }
            free(current);
            l->count--;
            return;
        }
        previous = current;
        current = current->next;
    }
}

// Removes all the elements from the list
void remove_all(List * l){
    ElementList * current = l->premier;
    ElementList * next;
    while(current != NULL){
        next = current->next;
        free(current);
        current = next;
    }
    l->premier = NULL;
    l->count = 0;
}

// Checks if an element is in the list
int is_in_list(List * l, char * name){
    ElementList * current = l->premier;
    while(current != NULL){
        if(strcmp(current->name, name) == 0){
            return 1;
        }
        current = current->next;
    }
    return 0;
}

// Checks if the list is empty
int is_empty(List * l){
    if(l->premier == NULL){
        return 1;
    }else{
        return 0;
    }
}

// Prints the list
void print_list(List * l){
    ElementList * current = l->premier;
    while(current != NULL){
        printf("%s\n", current->name);
        current = current->next;
    }
}


/*****************************************************
              Queue Type Def and Functions
******************************************************/

// This queue will hold elements of type pthread_t
typedef struct Queue Queue;
typedef struct Element Element;

struct Element{
    pthread_t number;
    Element *next;
};


struct Queue{
    Element * premier;
    int count;
};


// Creates a new queue
Queue * new_queue(){
    Queue * q = malloc(sizeof(Queue));
    q->premier = NULL;
    q->count = 0;
    return q;
}

// Adds an element to the queue
void enqueue(Queue * q, pthread_t number){
    Element * e = malloc(sizeof(Element));
    e->number = number;
    e->next = NULL;
    if(q->premier == NULL){
        q->premier = e;
    }else{
        Element * current = q->premier;
        while(current->next != NULL){
            current = current->next;
        }
        current->next = e;
    }
    q->count++;
}

// Removes the first element of the queue and returns it
// If the queue is empty, returns -1
pthread_t dequeue(Queue * q){
    if(q->premier == NULL){
        return -1;
    }else{
        Element * e = q->premier;
        q->premier = e->next;
        pthread_t number = e->number;
        free(e);
        q->count--;
        return number;
    }
}




/*****************************************************
              Global variables
******************************************************/



/***************************************
        Shared variables for clients
****************************************/

// Array of socket descriptors for the clients that are trying to connect
// The clients that will be in this array are clients
// that have the same username as a client that is already connected,
// or that haven't sent their username yet
int tab_client_connecting[MAX_CLIENT];

// Mutex to protect the tab_client_connecting array
pthread_mutex_t mutex_tab_client_connecting;

// Array of socket descriptors for the clients that are accepted by the server
// Accepted clients are clients that have sent their username
// and that have a unique username
int tab_client[MAX_CLIENT];

// Mutex to protect the tab_client array
pthread_mutex_t mutex_tab_client;

// Array of usernames for the clients that are accepted by the server
char tab_username[MAX_CLIENT][USERNAME_SIZE];

// Mutex to protect the tab_username array
pthread_mutex_t mutex_tab_username;

// Array of Lists for the channels that each client is in
List * tab_channel[MAX_CLIENT];

// Mutex to protect the tab_channel array
pthread_mutex_t mutex_tab_channel;

// A semaphore to indicate the number of free spots in the tab_client array
sem_t free_spot;

// The socket descriptor for the socket that deals with client connections
int dS;

// The socket descriptor for the socket that deals with file uploads
int upload_socket;

// Mutex to protect the upload_socket
pthread_mutex_t mutex_upload_socket;

// The socket descriptor for the socket that deals with file downloads
int download_socket;

// Mutex to protect the download_socket
pthread_mutex_t mutex_download_socket;

// A socket to deal with client channel connections and disconnections
int channel_socket;


/**************************************
      Shared variables for threads
***************************************/

// Array to store the threads id
pthread_t Threads_id [MAX_CLIENT];

// Mutex to protect the Threads_id array
pthread_mutex_t mutex_Threads_id;

// A semaphore to indicate when a thread has ended
sem_t thread_end;

// A shared queue to store the index of the clients who have disconnected
Queue * ended_threads;

// Mutex to protect the ended_threads queue
pthread_mutex_t mutex_ended_threads;



/**************************************
           Utility functions
***************************************/

// A function that will take as an argument the socket descriptor of the client
// and will return the indice of the client in the tab_client array

int get_indice_dSC(int dSC) {
    // Lock the mutex
    pthread_mutex_lock(&mutex_tab_client);
    int i = 0;
    while (i < MAX_CLIENT) {
        if (tab_client[i] == dSC) {
            // Unlock the mutex
            pthread_mutex_unlock(&mutex_tab_client);
            return i;
        }
        i = i + 1;
    }
    // If the client is not in the array, we return -1
    // Unlock the mutex
    pthread_mutex_unlock(&mutex_tab_client);
    return -1;
}


// A function that will take as an argument the socket descriptor of the client
// and will return the indice of the client in the tab_client_connecting array

int get_indice_dSC_connecting(int dSC){
    // Lock the mutex
    pthread_mutex_lock(&mutex_tab_client_connecting);
    int i = 0;
    while (i < MAX_CLIENT) {
        if (tab_client_connecting[i] == dSC) {
            // Unlock the mutex
            pthread_mutex_unlock(&mutex_tab_client_connecting);
            return i;
        }
        i = i + 1;
    }
    // If the client is not in the array, we return -1
    // Unlock the mutex
    pthread_mutex_unlock(&mutex_tab_client_connecting);
    return -1;
}


// A function that will take the username of a client as an argument
// and will return the indice of the client in the tab_client array
// If the client is not in the array, we return -1

int get_indice_username(char * username) {
    // Lock the mutex
    pthread_mutex_lock(&mutex_tab_username);
    int i = 0;
    while (i < MAX_CLIENT) {
        if (strcmp(tab_username[i], username) == 0) {
            // Unlock the mutex
            pthread_mutex_unlock(&mutex_tab_username);
            return i;
        }
        i = i + 1;
    }
    // If the client is not in the array, we return -1
    // Unlock the mutex
    pthread_mutex_unlock(&mutex_tab_username);
    return -1;
}

// A function that will find the indice of the first free spot in the tab_client_connecting array
// and will return it, or -1 if there is no free spot

int get_free_spot() {
    // Lock the mutex
    pthread_mutex_lock(&mutex_tab_client_connecting);
    int i = 0;
    while (i < MAX_CLIENT) {
        if (tab_client_connecting[i] == 0) {
            // Unlock the mutex
            pthread_mutex_unlock(&mutex_tab_client_connecting);
            return i;
        }
        i = i + 1;
    }
    // If there is no free spot, we return -1
    // Unlock the mutex
    pthread_mutex_unlock(&mutex_tab_client_connecting);
    return -1;
}

// Struct for the messages
typedef struct Message Message;
struct Message {
    // The command
    // For possible commands, please read the documentation
    char cmd[CMD_SIZE];
    // The username of the client who will receive the message
    // It can be Server if the message is sent from the server
    char from[USERNAME_SIZE];
    // The username of the client who sent the message
    // It can be Server if the message is sent to the server
    char to[USERNAME_SIZE];
    // The channel name
    char channel[CHANNEL_SIZE];
    // The message
    char message[MSG_SIZE];
    // The color of the message
    char color[COLOR_SIZE];
};


// A function that will take as an argument the index of the client
// and a pointer to a Message struct, and will send the message to all the clients
// except the client who sent the message

void send_to_all(int client_indice, Message * buffer) {
    int i = 0;
    int nb_send;
    // If the client_indice is -1, it means that the message is sent by the server
    if (client_indice != -1){
        // Lock the mutex
        pthread_mutex_lock(&mutex_tab_username);
        strcpy(buffer->from, tab_username[client_indice]);
        // Unlock the mutex
        pthread_mutex_unlock(&mutex_tab_username);
    }
    // Lock the mutex
    pthread_mutex_lock(&mutex_tab_client);
    pthread_mutex_lock(&mutex_tab_channel);

    printf("Channel sent to : %s by client : %d \n", buffer->channel, client_indice + 1);
    printf("Message sent : %s by client : %d \n\n", buffer->message, client_indice + 1);

    // If the channel is empty, we send to global
    if (strcmp(buffer->channel, "") == 0) {
        strcpy(buffer->channel, "global");
        // This shouldn't happen, so we print a warning
        printf("Warning: client has forgotten channel\n");
    }

    while( i < MAX_CLIENT){
        // We can't send the message to ourselves
        // also, if a client disconnects, we don't send the message to him
        if (tab_client[i] != 0 && i != client_indice) {
            // If the client is in the same channel as the channel from which the message was sent
            if(is_in_list(tab_channel[i], buffer->channel) == 1){
                nb_send = send(tab_client[i], buffer, BUFFER_SIZE, 0);
                if (nb_send == -1) {
                    perror("Erreur lors de l'envoi");
                    printf("L'erreur est dans le thread du client: %d\n", client_indice + 1);
                    exit(EXIT_FAILURE);
                }
                // If ever a client disconnect while we are sending the messages
                if (nb_send == 0) {
                    printf("Le client: %d s'est deconnecte, donc le message ne s'est pas envoye a lui\n", i + 1);
                    // We don't break here because we want to send the message to the other clients
                }
            }
        }
        i = i + 1;
    }
    // Unlock the mutex
    pthread_mutex_unlock(&mutex_tab_client);
    pthread_mutex_unlock(&mutex_tab_channel);
}

// A function that will handle the SIGINT signal, it will tell the clients that the server is closing
// and it will close the sockets, destroy the mutexes and semaphores, and free the memory

void handle_interrupt(int signum){
    printf("\nLe serveur va fermer\n");
    // We send a message to all the clients to tell them that the server is closing
    Message msg_buffer;
    Message * buffer = &msg_buffer;
    strcpy(buffer->cmd, "finserv");
    strcpy(buffer->from, "Serveur");
    strcpy(buffer->to, "all");
    strcpy(buffer->channel, "global");
    strcpy(buffer->message, "Le serveur va fermer. Au revoir!");
    send_to_all(-1, buffer);
    // We close the sockets
    close(dS);
    close(upload_socket);
    close(download_socket);
    close(channel_socket);
    // We close the sockets of all of the clients
    int i = 0;
    // Lock the mutex
    pthread_mutex_lock(&mutex_tab_client_connecting);
    pthread_mutex_lock(&mutex_Threads_id);
    while (i < MAX_CLIENT) {
        if (tab_client_connecting[i] != 0) {
            pthread_cancel(Threads_id[i]);
            close(tab_client_connecting[i]);
        }
        i = i + 1;
    }  
    // Unlock the mutex
    pthread_mutex_unlock(&mutex_tab_client_connecting);
    pthread_mutex_unlock(&mutex_Threads_id);

    printf("Socket clients fermes\n");

    // Destroy all the semaphores and mutexes
    sem_destroy(&free_spot);
    sem_destroy(&thread_end);
    pthread_mutex_destroy(&mutex_tab_client_connecting);
    pthread_mutex_destroy(&mutex_tab_client);
    pthread_mutex_destroy(&mutex_tab_username);
    pthread_mutex_destroy(&mutex_upload_socket);
    pthread_mutex_destroy(&mutex_download_socket);
    pthread_mutex_destroy(&mutex_Threads_id);
    pthread_mutex_destroy(&mutex_ended_threads);

    printf("Nettoyage termine\n");
    printf("Derniers reglages...\n");

    // Wait one second
    sleep(1);
    // We free the memory
    free(ended_threads);
    printf("Fermeture du serveur terminee avec success\n");
    // We exit the program
    exit(0);
}


/*********************************************
       Upload and Download Thread Functions
**********************************************/


// A function for a thread that will accept a connection using the socket 
// for uploads and will create and receive the file and write in the file

void * upload_file_thread(void * arg){
    int nb_recv; // The number of bytes received
    Message msg_buffer; // The buffer for the messages
    Message * buffer = &msg_buffer; // A pointer to the buffer
    int dS_thread_upload; // The socket for the upload for the accept
    int continue_thread = 1; // A variable to know if we continue the thread or not
    long file_size; // The size of the file
    char path[MSG_SIZE]; // The path of the file
    FILE * file; // The file

    pthread_t ThreadId = pthread_self(); // The id of the thread, will be used to cleanup thread once finished

    // Initialise file_addr and length
    struct sockaddr_in file_addr;
    socklen_t length_file_addr;

    // We accept the connection
    length_file_addr = sizeof(struct sockaddr_in);
    dS_thread_upload = accept(upload_socket, (struct sockaddr*) &file_addr,&length_file_addr) ;
    if (dS_thread_upload == -1) {
        perror("Erreur lors de l'accept");
        exit(EXIT_FAILURE);
    }

    // We receive the name of the file
    nb_recv = recv(dS_thread_upload, buffer, BUFFER_SIZE, 0);
    if (nb_recv == -1) {
        perror("Erreur lors de la reception upload client");
        exit(EXIT_FAILURE);
    }
    // If ever a client disconnect while we are receiving the messages
    if (nb_recv == 0) {
        printf("Le client s'est deconnecte dans le file upload\n");
        continue_thread = 0;
    }

    // File creation
    if (continue_thread == 1){
        printf("Le nom du fichier est: %s\n", buffer->message);

        // We create the file
        strcpy(path, "../src/server_files/");
        strcat(path, buffer->message);
        file = fopen(path, "w");
        if (file == NULL) {
            perror("Erreur lors de la creation du fichier");
            continue_thread = 0;
            exit(EXIT_FAILURE);
        }
    }

    // We receive the size of the file
    if (continue_thread == 1){
        printf("Le fichier %s a ete cree\n", buffer->message);

        // We receive the size of the file
        nb_recv = recv(dS_thread_upload, &file_size, sizeof(long), 0);
        if (nb_recv == -1) {
            perror("Erreur lors de la reception upload client");
            exit(EXIT_FAILURE);
        }
        // If ever a client disconnect while we are receiving the messages
        if (nb_recv == 0) {
            printf("Le client s'est deconnecte dans le file upload\n");
            continue_thread = 0;
        }
    }

    // Packet for the file data
    char packet[BUFFER_SIZE];
    int nb_recv_total = 0;
    
    // We receive the data in the file
    if (continue_thread == 1){
        while(1){
            nb_recv = recv(dS_thread_upload, packet, BUFFER_SIZE, 0);
            if (nb_recv == -1) {
                perror("Erreur lors de la reception");
                exit(EXIT_FAILURE);
            }
            // If ever a client disconnect while we are receiving the messages
            // or if he has sent everything and closed the socket
            if (nb_recv == 0) {
                printf("Socket ferme : Le client a fini de upload\n");
                break;
            }
            nb_recv_total = nb_recv_total + nb_recv;

            
            // If the nb_recv_total greater than the file_size, we stop the loop
            if (nb_recv_total >= file_size + 1000) {
                break;
            }
                    
            // We write in the file
            fwrite(packet, sizeof(char), nb_recv, file);
        }
        // We close the file
        fclose(file);
        printf("Le fichier a ete ferme\n");
        printf("nb_recv_total: %d\n", nb_recv_total);
        printf("La taille du fichier est: %ld\n", file_size);
    }

    // We close the socket
    close(dS_thread_upload);
    
    // We put the ThreadId in the queue
    // Lock the mutex
    pthread_mutex_lock(&mutex_ended_threads);
    enqueue(ended_threads, ThreadId);
    // Unlock the mutex
    pthread_mutex_unlock(&mutex_ended_threads);

    // We increment the semaphore
    sem_post(&thread_end);

    printf("Thread upload termine\n");
    // We exit the thread
    pthread_exit(0);
}



// A function for a thread that will accept a connection using the socket 
// for downloads and will send the list of files available for download.
// Once the client has chosen a file, and sent back the file he chose,
// the thread will send the size of the file and the file itself.

void * download_file_thread(void * arg){

    int nb_recv; // The number of bytes received
    int nb_send; // The number of bytes sent
    Message msg_buffer; // The buffer for the messages
    Message * buffer = &msg_buffer; // A pointer to the buffer
    int dS_thread_download; // The socket for the download for the accept
    int continue_thread = 1; // A variable to know if we continue the thread or not
    long file_size; // The size of the file
    char path[MSG_SIZE]; // The path of the file
    FILE * file; // The file

    pthread_t ThreadId = pthread_self(); // The id of the thread, will be used to cleanup thread once finished

    // Initialise file_addr and length
    struct sockaddr_in file_addr;
    socklen_t length_file_addr;

    // We accept the connection
    dS_thread_download = accept(download_socket, (struct sockaddr *) &file_addr, &length_file_addr);
    if (dS_thread_download == -1) {
        perror("Erreur lors de l'accept");
        exit(EXIT_FAILURE);
    }

    // Directory path
    const char* directory_path = "../src/server_files/";

    // Open the directory
    DIR* directory = opendir(directory_path);
    if (directory == NULL) {
        printf("Unable to open directory.\n");
        continue_thread = 0;
    }
    
    if (continue_thread == 1){
        // Read directory entries
        struct dirent* entry;
        char file_list[MSG_SIZE];

        // Put \0 at the beginning of the message to avoid concatenation problems
        buffer->message[0] = '\0';


        while ((entry = readdir(directory)) != NULL) {
            // Exclude "." and ".." directories
            if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
                // Copy the file name to the file_list
                strncpy(file_list, entry->d_name, sizeof(file_list));
                file_list[sizeof(file_list) - 1] = '\0';  // Ensure null-termination

                // Concatentate the file_list to buffer->message
                strcat(buffer->message, file_list);
                strcat(buffer->message, "/");

            }
        }

        // Close the directory
        closedir(directory);

        // We send the list of files
        strcpy(buffer->cmd, "download");
        strcpy(buffer->to, buffer->from);
        strcpy(buffer->from, "Serveur");
        nb_send = send(dS_thread_download, buffer, BUFFER_SIZE, 0);
        if (nb_send == -1) {
            perror("Erreur lors de l'envoi");
            exit(EXIT_FAILURE);
        }
        // If the client disconnected, we stop the thread
        if (nb_send == 0) {
            printf("Le client s'est deconnecte dans le download\n");
            continue_thread = 0;
        }
    }

    if (continue_thread == 1){
        // We receive the file name the client wants to download
        nb_recv = recv(dS_thread_download, buffer, BUFFER_SIZE, 0);
        if (nb_recv == -1) {
            perror("Erreur lors de la reception");
            exit(EXIT_FAILURE);
        }
        // If the client disconnected, we stop the thread
        if (nb_recv == 0) {
            continue_thread = 0;
        }
        
        // If the buffer->cmd is "cancel" we stop the thread
        if (strcmp(buffer->cmd, "cancel") == 0) {
            continue_thread = 0;
        }

    }

    if (continue_thread == 1){
        // We concatenate the path of the file
        strcpy(path, "../src/server_files/");
        strcat(path, buffer->message);

        // We open the file
        file = fopen(path, "rb");
        if (file == NULL) {
            perror("Erreur lors de l'ouverture du fichier");
            continue_thread = 0;
        }

    }

    if (continue_thread == 1){
        printf("Le fichier %s a ete ouvert\n", buffer->message);
        // We get the size of the file
        // Put the cursor at the end of the file
        fseek(file, 0, SEEK_END);  
        // Get the size of the file
        file_size = ftell(file);   
        // Put the cursor at the beginning of the file     
        rewind(file);              

        // We send the size of the file
        nb_send = send(dS_thread_download, &file_size, sizeof(long), 0);
        if (nb_send == -1) {
            perror("Erreur lors de l'envoi");
            exit(EXIT_FAILURE);
        } 
        if (nb_send == 0) {
            printf("Le client s'est deconnecte lors de l'envoi de file_size\n");
            continue_thread = 0;
        }
    }

    char packet[BUFFER_SIZE];
    int nb_read_total = 0;
    int nb_read = 0;
    //printf("Envoie du fichier au serveur\n");
    if (continue_thread == 1){
        // Envoie le fichier au serveur
        while(nb_read_total < file_size){
            nb_read = fread(packet, 1, BUFFER_SIZE, file);
            nb_read_total += nb_read;
            // ajouter /0 a la fin du buffer si le fichier est plus petit que BUFFER_SIZE
            if (nb_read < BUFFER_SIZE){ // inutile mais pour un code robuste
                packet[nb_read] = '\0';
            }
            //printf("Taille du fichier : %ld, Taille lu: %d\n, Taille lu total:%d\n", size_file, nb_read, nb_read_total);
            if (nb_read < BUFFER_SIZE){
                nb_send = send(dS_thread_download, packet, nb_read, 0);
            }else{
                nb_send = send(dS_thread_download, packet, BUFFER_SIZE, 0);
            }
            //printf("Taille envoye: %d\n", nb_send);
            //printf("Message envoye: %s\n", buffer);
            if (nb_send == -1) {
                perror("Erreur lors de l'envoi du message");
                exit(EXIT_FAILURE);
            }
            if (nb_send == 0) {
                printf("Le client s'est deconnecte lors de l'envoi du fichier\n");
                continue_thread = 0;
                break;
            }
            bzero(packet, BUFFER_SIZE);
        }

        // We close the file
        fclose(file);
        printf("Le fichier a ete ferme\n");

    }

    // We close the socket
    close(dS_thread_download);
    
    // Lock the mutex
    pthread_mutex_lock(&mutex_ended_threads);
    // We put the thread id in the queue of ended threads
    enqueue(ended_threads, ThreadId);
    // Unlock the mutex
    pthread_mutex_unlock(&mutex_ended_threads);

    // Increment the semaphore to indicate that a thread has ended
    sem_post(&thread_end);

    printf("Download thread end\n");
    pthread_exit(0);
} 


// A function for a thread that will send the list of channels available
// and let the client connect and disconnect from channels
// The client will also be able to create and delete a channel

void * channel_thread(void * arg){

    int indice_client = *(int *) arg; // The indice of the client in the tab_client array

    int nb_recv; // The number of bytes received
    int nb_send; // The number of bytes sent
    Message msg_buffer; // The buffer for the messages
    Message * buffer = &msg_buffer; // A pointer to the buffer
    int dS_thread_channel; // The socket for the download for the accept
    int continue_thread = 1; // A variable to know if we continue the thread or not

    pthread_t ThreadId = pthread_self(); // The id of the thread, will be used to cleanup thread once finished

    // Initialise file_addr and length
    struct sockaddr_in file_addr;
    socklen_t length_file_addr;

    // We accept the connection
    dS_thread_channel = accept(channel_socket, (struct sockaddr *) &file_addr, &length_file_addr);
    if (dS_thread_channel == -1) {
        perror("Erreur lors de l'accept");
        exit(EXIT_FAILURE);
    }

    // Directory path
    const char* directory_path = "../src/server_channels/";

    // Open the directory
    DIR* directory = opendir(directory_path);
    if (directory == NULL) {
        printf("Unable to open directory.\n");
        continue_thread = 0;
    }
    
    if (continue_thread == 1){
        // Read directory entries
        struct dirent* entry;
        char file_list[MSG_SIZE];

        // Put \0 at the beginning of the message to avoid concatenation problems
        buffer->message[0] = '\0';

        // Concatenate a "/" at the beginning of the message
        strcat(buffer->message, "/");


        while ((entry = readdir(directory)) != NULL) {
            // Exclude "." and ".." directories
            if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
                // Copy the channel name to the file_list
                strncpy(file_list, entry->d_name, sizeof(file_list));
                file_list[sizeof(file_list) - 1] = '\0';  // Ensure null-termination

                // Check if the user is in the channel
                // If he is, we add a * at the start of the channel name
                if (is_in_list(tab_channel[indice_client], file_list) == 1){
                    strcat(buffer->message, "*");
                }

                // Concatentate the file_list to buffer->message
                strcat(buffer->message, file_list);
                strcat(buffer->message, "/");

            }
        }

        // Close the directory
        closedir(directory);

        // We send the list of channels
        strcpy(buffer->cmd, "salon");
        strcpy(buffer->to, buffer->from);
        strcpy(buffer->from, "Serveur");
        nb_send = send(dS_thread_channel, buffer, BUFFER_SIZE, 0);
        if (nb_send == -1) {
            perror("Erreur lors de l'envoi");
            exit(EXIT_FAILURE);
        }
        // If the client disconnected, we stop the thread
        if (nb_send == 0) {
            printf("Le client s'est deconnecte dans le channel co/deco\n");
            continue_thread = 0;
        }
    }

    if (continue_thread == 1){
        while(1){
            // We receive the a message from the client
            nb_recv = recv(dS_thread_channel, buffer, BUFFER_SIZE, 0);
            if (nb_recv == -1) {
                perror("Erreur lors de la reception");
                exit(EXIT_FAILURE);
            }
            // If the client disconnected, we stop the thread
            if (nb_recv == 0) {
                printf("Le client s'est deconnecte dans le channel co/deco\n");
                continue_thread = 0;
                break;
            }
            
            // If the buffer->cmd is "exitm" we stop the thread
            if (strcmp(buffer->cmd, "exitm") == 0) {
                printf("Le client a quitte le menu channel\n");
                continue_thread = 0;
                break;
            }

            // If the buffer->cmd is "connect" we add the client to the channel
            if (strcmp(buffer->cmd, "connect") == 0) {
                // Lock the mutex
                pthread_mutex_lock(&mutex_tab_channel);
                // We add the client to the channel
                add(tab_channel[indice_client], buffer->channel);
                // Unlock the mutex
                pthread_mutex_unlock(&mutex_tab_channel);
                printf("Le client %d a rejoint le channel %s\n", indice_client + 1, buffer->channel);
                // Send a message to all the clients in the channel to tell them that the client has joined
                strcpy(buffer->cmd, "");
                strcpy(buffer->to, "all");
                strcpy(buffer->from, "Serveur");
                strcpy(buffer->message, "Je rejoins le channel");
                send_to_all(indice_client, buffer);
            }

            // If the buffer->cmd is "disc" we remove the client from the channel
            if (strcmp(buffer->cmd, "disc") == 0) {
                // Lock the mutex
                pthread_mutex_lock(&mutex_tab_channel);
                // We remove the client from the channel
                remove_element(tab_channel[indice_client], buffer->channel);
                // Unlock the mutex
                pthread_mutex_unlock(&mutex_tab_channel);
                printf("Le client %d a quitte le channel %s\n", indice_client + 1, buffer->channel);
                // Send a message to all the clients in the channel to tell them that the client has left
                strcpy(buffer->cmd, "");
                strcpy(buffer->to, "all");
                strcpy(buffer->from, "Serveur");
                strcpy(buffer->message, "Je quitte le channel");
                send_to_all(indice_client, buffer);
            }

            // If the buffer->cmd is "create" we create a channel
            // The client sends us the name of the channel he wants to create in buffer->channel
            // and then a description of the channel in buffer->message in the next message
            if (strcmp(buffer->cmd, "create") == 0) {
                char path[MSG_SIZE]; // The path of the file
                FILE * file; // The file
                // We concatenate the path of the file
                strcpy(path, "../src/server_channels/");
                strcat(path, buffer->channel);

                // We open the file
                file = fopen(path, "w");
                if (file == NULL) {
                    perror("Erreur lors de la creation du fichier");
                    continue_thread = 0;
                }

                // We receive the description of the channel
                nb_recv = recv(dS_thread_channel, buffer, BUFFER_SIZE, 0);
                if (nb_recv == -1) {
                    perror("Erreur lors de la reception");
                    exit(EXIT_FAILURE);
                }
                // If the client disconnected, we stop the thread
                if (nb_recv == 0) {
                    printf("Le client s'est deconnecte dans le channel co/deco\n");
                    fclose(file);
                    continue_thread = 0;
                    break;
                }

                // We write the description of the channel in the file
                fprintf(file, "%s", buffer->message);

                // We close the file
                fclose(file);
                printf("Le channel %s a ete cree\n", buffer->channel);

                // We add the client to the channel
                // Lock the mutex
                pthread_mutex_lock(&mutex_tab_channel);
                add(tab_channel[indice_client], buffer->channel);
                // Unlock the mutex
                pthread_mutex_unlock(&mutex_tab_channel);
                printf("Le client %d a rejoint le channel %s\n", indice_client + 1, buffer->channel);

                // We message all of the clients in the global channel to tell them that a new channel has been created
                strcpy(buffer->cmd, "");
                strcpy(buffer->to, "all");
                strcpy(buffer->from, "Serveur");
                sprintf(buffer->message, "Le nouveau channel: %s a ete cree", buffer->channel);
                strcpy(buffer->channel, "global");
                send_to_all(-1, buffer);

                // Once the channel is created, the client is no longer in the menu
                continue_thread = 0;
                break;
            }

            // If the buffer->cmd is "delete" we delete a channel
            // The client sends us the name of the channel he wants to delete in buffer->channel
            if (strcmp(buffer->cmd, "delete") == 0) {
                char path[MSG_SIZE]; // The path of the file
                char channel_to_delete[CHANNEL_SIZE]; // The name of the channel to delete
                // We concatenate the path of the file
                strcpy(path, "../src/server_channels/");
                strcat(path, buffer->channel);
                strcpy(channel_to_delete, buffer->channel);

                // We delete the file
                if (remove(path) == 0) {
                    printf("Le channel %s a ete supprime\n", buffer->channel);
                }
                else {
                    printf("Erreur lors de la suppression du channel %s\n", buffer->channel);
                }


                // We need to send a message to all the clients in the channel to tell them that the channel has been deleted
                strcpy(buffer->cmd, "end");
                strcpy(buffer->to, "all");
                strcpy(buffer->from, "Serveur");
                strcpy(buffer->message, "Le channel a ete supprime");
                send_to_all(-1, buffer);

                printf("Le client %d a supprimer le channel %s\n", indice_client + 1, buffer->channel);
                // We need to send a message to all the clients in the global channel to tell them that a channel has been deleted
                strcpy(buffer->cmd, "");
                strcpy(buffer->to, "all");
                strcpy(buffer->from, "Serveur");
                sprintf(buffer->message, "Le channel %s a ete supprime", buffer->channel);
                strcpy(buffer->channel, "global");
                send_to_all(-1, buffer);

                // We remove the channel from all the clients
                int i = 0;
                // Lock the mutex
                pthread_mutex_lock(&mutex_tab_channel);
                pthread_mutex_lock(&mutex_tab_client);
                while (i < MAX_CLIENT) {
                    if (tab_client[i] != 0) {
                        remove_element(tab_channel[i], channel_to_delete);
                    }
                    i = i + 1;
                }
                // Unlock the mutex
                pthread_mutex_unlock(&mutex_tab_channel);
                pthread_mutex_unlock(&mutex_tab_client);

                // Once the channel is deleted, the client is no longer in the menu
                continue_thread = 0;
                break;
            }

        }
    }

    // We close the socket
    close(dS_thread_channel);
    
    // Lock the mutex
    pthread_mutex_lock(&mutex_ended_threads);
    // We put the thread id in the queue of ended threads
    enqueue(ended_threads, ThreadId);
    // Unlock the mutex
    pthread_mutex_unlock(&mutex_ended_threads);

    // Increment the semaphore to indicate that a thread has ended
    sem_post(&thread_end);

    printf("Channel connect/disconnect thread end\n");
    pthread_exit(0);

}




/*******************************************
        Main Thread Function for Clients
*********************************************/


// A function for a thread that will take as an argument
// the socket descriptor of the client, and will receive messages
// from the client and redirect them to the right client/s

void * client_thread(void * dS_client_connection) {

    Message msg_buffer; // The buffer to store the message
    Message * buffer = &msg_buffer; // A pointer to the buffer
    int nb_recv;
    int nb_send;
    int i;
    int client_indice;
    int client_indice_connecting;
    pthread_t ThreadId;

    // Variable that will allow us to know if we can continue the thread
    // This variable will be set to 0 if the client disconnects while he is giving his username
    // It will cause the while loops to stop
    int continue_thread = 1;

    /********************************
        Unique Username Management
    *********************************/
    // Lock the mutex
    pthread_mutex_lock(&mutex_tab_client_connecting);
    int dSC_connection = *(int *)dS_client_connection; // The socket descriptor of the client
    // Unlock the mutex
    pthread_mutex_unlock(&mutex_tab_client_connecting);
    client_indice_connecting = get_indice_dSC_connecting(dSC_connection); // The indice of the client in the tab_client_connecting array

    // While the client has not provided a unique username, 
    // we do not put him in the tab_client array
    // Everytime he send a username, we check if it is unique
    // If it is not, we send him false and he has to send another username
    // If it is, we send him true and we put him in the tab_client array

    while (continue_thread == 1) {
        // We receive the message from the client
        nb_recv = recv(dSC_connection, buffer, BUFFER_SIZE, 0);
        if (nb_recv == -1) {
            perror("Erreur lors de la reception");
            printf("L'erreur est dans le thread du client : %d\n", client_indice_connecting + 1);
            exit(EXIT_FAILURE);
        }
        if (nb_recv == 0) {
            printf("Client %d s'est deconnecte\n", client_indice_connecting + 1);
            // Here, the client has disconnected before even providing a valid username
            // This will ensure that the next while loop will not be executed
            continue_thread = 0;
            break;
        }

        // We check if the username is unique
        // If it is, we put the client in the tab_client array
        // If it is not, we send him false and he has to send another username
        if (get_indice_username(buffer->from) == -1) {
            // We put the client in the tab_client array
            // Lock the mutex because we are going to write in the tab_client array
            pthread_mutex_lock(&mutex_tab_client);
            tab_client[client_indice_connecting] = dSC_connection;
            // Unlock the mutex
            pthread_mutex_unlock(&mutex_tab_client);
            // We put the username in the tab_username array
            // Lock the mutex because we are going to write in the tab_username array
            pthread_mutex_lock(&mutex_tab_username);
            strcpy(tab_username[client_indice_connecting], buffer->from);
            // Unlock the mutex
            pthread_mutex_unlock(&mutex_tab_username);
            // We send true to the client
            strcpy(buffer->to, buffer->from);
            strcpy(buffer->from, "Server");
            strcpy(buffer->message, "true");
            nb_send = send(dSC_connection, buffer, BUFFER_SIZE, 0);
            if (nb_send == -1) {
                perror("Erreur lors de l'envoi");
                printf("L'erreur est dans le thread du client : %d\n", client_indice_connecting + 1);
                exit(EXIT_FAILURE);
            }
            break;
        } 
        else {
            // We send false to the client
            strcpy(buffer->to, buffer->from);
            strcpy(buffer->from, "Server");
            strcpy(buffer->message, "false");
            nb_send = send(dSC_connection, buffer, BUFFER_SIZE, 0);
            if (nb_send == -1) {
                perror("Erreur lors de l'envoi");
                printf("L'erreur est dans le thread du client : %d\n", client_indice_connecting + 1);
                exit(EXIT_FAILURE);
            }
        }


    }

    // Get the pointer to the place in the tab_client array where the socket descriptor of the client is stored
    int * dS_client = &tab_client[client_indice_connecting];
    client_indice = client_indice_connecting;


    /***************************************
        Communication with other clients
    ****************************************/
    int dSC = dSC_connection; // The socket descriptor of the client

    if (continue_thread == 1) {
        // We tell the other clients that a new client has connected
        // Lock the mutex
        pthread_mutex_lock(&mutex_tab_username);
        strcpy(buffer->from, tab_username[client_indice]);
        strcpy(buffer->to, "all");
        // Unlock the mutex
        pthread_mutex_unlock(&mutex_tab_username);
        strcpy(buffer->channel, "global");
        strcpy(buffer->message, "Je me connecte. Bonjour!");
        send_to_all(client_indice, buffer);
    }

    while (continue_thread == 1) {

        // We receive the message from the client
        nb_recv = recv(dSC, buffer, BUFFER_SIZE, 0);
        if (nb_recv == -1) {
            perror("Erreur lors de la reception");
            printf("L'erreur est dans le thread du client : %d\n", client_indice + 1);
            exit(EXIT_FAILURE);
        }
        if (nb_recv == 0) {
            printf("Client %d s'est deconnecte\n", client_indice + 1);
            strcpy(buffer->channel, "global");
            strcpy(buffer->message, "Je me deconnecte. Au revoir!");
            // We send a message to the other clients to tell them that this client has disconnected
            send_to_all(client_indice, buffer);
            break;
        }

        printf("Message received: %s by client: %d \n", buffer->message, client_indice + 1);

        // If the client sends "fin", we break and close his socket
        if (strcmp(buffer->cmd, "fin") == 0) {
            printf("Fin de la discussion pour client: %d\n", client_indice + 1);
            strcpy(buffer->channel, "global");
            strcpy(buffer->message, "Je me deconnecte. Au revoir!");
            // We send a message to the other clients to tell them that this client has disconnected
            send_to_all(client_indice, buffer);
            break;
        }

        // If the client sends "list", we send him the list of the connected clients
        if (strcmp(buffer->cmd, "list") == 0) {
            strcpy(buffer->to, buffer->from);
            strcpy(buffer->from, "Serveur");
            strcpy(buffer->cmd, "list");
            char list[MSG_SIZE];
            strcpy(list, "Liste des clients connectes: \n");
            // Lock the mutexs
            pthread_mutex_lock(&mutex_tab_client);
            pthread_mutex_lock(&mutex_tab_username);
            i = 0;
            while (i < MAX_CLIENT) {
                if (tab_client[i] != 0) {
                    strcat(list, tab_username[i]);
                    strcat(list, "\n");
                }
                i = i + 1;
            }
            // Unlock the mutexs
            pthread_mutex_unlock(&mutex_tab_client);
            pthread_mutex_unlock(&mutex_tab_username);
            strcpy(buffer->message, list);
            nb_send = send(dSC, buffer, BUFFER_SIZE, 0);
            if (nb_send == -1) {
                perror("Erreur lors de l'envoi");
                printf("L'erreur est dans le thread du client: %d\n", client_indice + 1);
                exit(EXIT_FAILURE);
            }
            continue;
        }

        // If the client sends "who", we send him his username
        if (strcmp(buffer->cmd, "who") == 0) {
            strcpy(buffer->from, "Serveur");
            strcpy(buffer->cmd, "who");
            // Lock the mutex
            pthread_mutex_lock(&mutex_tab_username);
            strcpy(buffer->message, tab_username[client_indice]);
            strcpy(buffer->to, tab_username[client_indice]);
            // Unlock the mutex
            pthread_mutex_unlock(&mutex_tab_username);
            nb_send = send(dSC, buffer, BUFFER_SIZE, 0);
            if (nb_send == -1) {
                perror("Erreur lors de l'envoi");
                printf("L'erreur est dans le thread du client: %d\n", client_indice + 1);
                exit(EXIT_FAILURE);
            }
            continue;
        }

        // If the client sends "dm", we send the message to the person who's username is in buffer.to
        if (strcmp(buffer->cmd, "dm") == 0) {
            // We get the indice of the client to send the message to
            int client_to_send = get_indice_username(buffer->to);
            // If the client is not in the array, we send an error message to the client
            if (client_to_send == -1) {
                strcpy(buffer->to, buffer->from);
                strcpy(buffer->from, "Serveur");
                strcpy(buffer->cmd, "error");
                strcpy(buffer->message, "Le client n'existe pas");
                nb_send = send(dSC, buffer, BUFFER_SIZE, 0);
                if (nb_send == -1) {
                    perror("Erreur lors de l'envoi");
                    printf("L'erreur est dans le thread du client: %d\n", client_indice + 1);
                    exit(EXIT_FAILURE);
                }
                continue;
            }
            // If the client is in the array, we send him the message
            strcpy(buffer->cmd, "dm");
            // Lock the mutex
            pthread_mutex_lock(&mutex_tab_client);
            nb_send = send(tab_client[client_to_send], buffer, BUFFER_SIZE, 0);
            // Unlock the mutex
            pthread_mutex_unlock(&mutex_tab_client);
            if (nb_send == -1) {
                perror("Erreur lors de l'envoi");
                printf("L'erreur est dans le thread du client: %d\n", client_indice + 1);
                exit(EXIT_FAILURE);
            }
            continue;
        }

        // If the client sends "upload", we create a thread to receive the file
        if (strcmp(buffer->cmd, "upload") == 0) {
            printf("UPLOAD detected\n");

            // We launch a thread to receive the file
            pthread_t thread_upload;

            if (pthread_create(&thread_upload, NULL, upload_file_thread, NULL) != 0) {
                perror("Erreur lors de la creation du thread upload");
                printf("L'erreur est dans le thread du client: %d\n", client_indice + 1);
                exit(EXIT_FAILURE);
            }
            printf("Thread upload cree\n");
           
            // We send a message to the other clients to tell them that this client has uploaded a file
            strcpy(buffer->cmd, "upload");
            strcpy(buffer->channel, "global");
            strcpy(buffer->message, "I am uploading a file!");
            send_to_all(client_indice, buffer);
            continue;
        }

        // If the client sends "download", we launch a thread to send the file
        if (strcmp(buffer->cmd, "download") == 0) {
            printf("DOWNLOAD detected\n");

            // We launch a thread to send the file
            pthread_t thread_download;

            if (pthread_create(&thread_download, NULL, download_file_thread, NULL) != 0) {
                perror("Erreur lors de la creation du thread download");
                printf("L'erreur est dans le thread du client: %d\n", client_indice + 1);
                exit(EXIT_FAILURE);
            }
            printf("Thread download cree\n");
            continue;
        }

        // If the client sens "salon", we launch a thread to send him the list of channels
        // And let him connect and disconnect freely
        if (strcmp(buffer->cmd, "salon") == 0) {
            printf("SALON detected\n");

            // We launch a thread to send the list of channels
            pthread_t thread_salon;

            if (pthread_create(&thread_salon, NULL, channel_thread, (void *) &client_indice) != 0) {
                perror("Erreur lors de la creation du thread salon");
                printf("L'erreur est dans le thread du client: %d\n", client_indice + 1);
                exit(EXIT_FAILURE);
            }
            printf("Thread salon cree\n");
            continue;
        }

        // If the client sends "exit", we exit the channel that he specified in buffer->channel
        if (strcmp(buffer->cmd, "exit") == 0) {
            printf("EXIT detected\n");
            // Lock the mutex
            pthread_mutex_lock(&mutex_tab_channel);
            remove_element(tab_channel[client_indice], buffer->channel);
            // Unlock the mutex
            pthread_mutex_unlock(&mutex_tab_channel);
            printf("Le client %d a quitte le channel %s\n", client_indice + 1, buffer->channel);
            // We send a message to the other clients in the channel to tell them that this client has exited the channel
            strcpy(buffer->cmd, "");
            strcpy(buffer->message, "Je quitte le channel");
            send_to_all(client_indice, buffer);
            // We remove the channel from the list of channels of the client
            continue;
        }


        // By default, we send the message to all the clients connected,
        // using the function send_to_all
        send_to_all(client_indice, buffer);
        
    }

    /**************************
           End of thread
    ***************************/

    // We close the socket of the client who wanted to disconnect
    if (close(dSC) == -1) {
        perror("Erreur lors de la fermeture du descripteur de fichier");
    }

    // We put 0 in the tab_client array
    // Lock the mutex
    pthread_mutex_lock(&mutex_tab_client);
    *(int *)dS_client = 0;
    // Unlock the mutex
    pthread_mutex_unlock(&mutex_tab_client);

    // We put 0 in the tab_client_connecting array
    // Lock the mutex
    pthread_mutex_lock(&mutex_tab_client_connecting);
    *(int *)dS_client_connection = 0;
    // Unlock the mutex
    pthread_mutex_unlock(&mutex_tab_client_connecting);

    // We clear the username of the client
    // Lock the mutex
    pthread_mutex_lock(&mutex_tab_username);
    strcpy(tab_username[client_indice], "");
    // Unlock the mutex
    pthread_mutex_unlock(&mutex_tab_username);

    // We get the thread id of the thread that is ending
    // Lock the mutex
    pthread_mutex_lock(&mutex_Threads_id);
    ThreadId = Threads_id[client_indice];
    // Put 0 in the tab_thread_id array
    Threads_id[client_indice] = 0;
    // Unlock the mutex
    pthread_mutex_unlock(&mutex_Threads_id);

    // We reset the channel list of the client
    // Lock the mutex
    pthread_mutex_lock(&mutex_tab_channel);
    remove_all(tab_channel[client_indice]);
    add(tab_channel[client_indice], "global");
    // Unlock the mutex
    pthread_mutex_unlock(&mutex_tab_channel);

    // We put the thread id in the shared queue of ended threads
    // Lock the mutex
    pthread_mutex_lock(&mutex_ended_threads);
    enqueue(ended_threads, ThreadId);
    // Unlock the mutex
    pthread_mutex_unlock(&mutex_ended_threads);

    // We increment the semaphore for thread cleanup
    sem_post(&thread_end);
    // We increment the semaphore to indicate that there is a free spot in the tab_client array
    sem_post(&free_spot);

    pthread_exit(0);
}




/***************************************
        Thread Cleanup Handler
****************************************/

// A function for a thread that will cleanup ended client threads
// The thread that cleans up sleeps until a client thread ends
// It checks a semaphore to see if a client thread has ended
// If a client thread has ended, it joins the thread and cleans up the thread
// It gets the if of the thread that ended from the shared queue of ended threads

void * cleanup(void * arg) {

    while (1) {
        sem_wait(&thread_end);
        pthread_t thread_id;
        // We get the id of the thread that ended from the shared queue of ended threads
        // Lock the mutex
        pthread_mutex_lock(&mutex_ended_threads);
        thread_id = dequeue(ended_threads);
        // Unlock the mutex
        pthread_mutex_unlock(&mutex_ended_threads);
        if (thread_id == -1) {
            perror("ERREUR CRITIQUE DEQUEUE");
            exit(EXIT_FAILURE);
        }
        // We join the thread        
        if (pthread_join(thread_id, NULL) == -1){
            perror("Erreur lors du join d'un thread");
        }
        else{
            printf("Thread %ld joined\n", thread_id);
        }
    }

    pthread_exit(0);
}



/*********************************
          Main function
**********************************/


int main(int argc, char *argv[]) {

  if (argc != 2) {
        // We check if the user provided exactly 1 argument
        printf("Error: You must provide exactly 1 argument.\nUsage: ./serv <port>\n");
        exit(EXIT_FAILURE);
    }  

  printf("Debut du Serveur.\n");

  // Creation of the sockets

  dS = socket(PF_INET, SOCK_STREAM, 0);
  if(dS == -1) {
    perror("Erreur lors de la creation du socket");
    exit(EXIT_FAILURE);
  }
  printf("Socket cree\n");


  upload_socket = socket(PF_INET, SOCK_STREAM, 0);
  if(upload_socket == -1) {
    perror("Erreur lors de la creation du socket");
    exit(EXIT_FAILURE);
  }
  printf("Socket cree\n");

  download_socket = socket(PF_INET, SOCK_STREAM, 0);
  if(download_socket == -1) {
    perror("Erreur lors de la creation du socket");
    exit(EXIT_FAILURE);
  }
  printf("Socket cree\n");

  channel_socket = socket(PF_INET, SOCK_STREAM, 0);
    if(channel_socket == -1) {
        perror("Erreur lors de la creation du socket");
        exit(EXIT_FAILURE);
    }
    printf("Socket cree\n");

  // Voici la doc des structs utilises
  /*
  struct sockaddr_in {
    sa_family_t    sin_family;  famille d'adresses : AF_INET     
    uint16_t       sin_port;    port dans l'ordre d'octets reseau
    struct in_addr sin_addr;    adresse Internet                 
  };

 Adresse Internet 
  struct in_addr {
    uint32_t    s_addr;   Adresse dans l'ordre d'octets reseau 
  };
  */

  // Nommage

    struct sockaddr_in ad;
    ad.sin_family = AF_INET;
    ad.sin_addr.s_addr = INADDR_ANY ;
    ad.sin_port = htons(atoi(argv[1])) ;
    if(bind(dS, (struct sockaddr*)&ad, sizeof(ad)) == -1) {
        perror("Erreur lors du nommage du socket");
        exit(EXIT_FAILURE);
    }
    printf("Socket message nomme\n");

    struct sockaddr_in ad_upload;
    ad_upload.sin_family = AF_INET;
    ad_upload.sin_addr.s_addr = INADDR_ANY ;
    ad_upload.sin_port = htons(atoi(argv[1]) + 1) ;
    if(bind(upload_socket, (struct sockaddr*)&ad_upload, sizeof(ad_upload)) == -1) {
        perror("Erreur lors du nommage du socket");
        exit(EXIT_FAILURE);
    }
    printf("Socket upload nomme\n");

    struct sockaddr_in ad_download;
    ad_download.sin_family = AF_INET;
    ad_download.sin_addr.s_addr = INADDR_ANY ;
    ad_download.sin_port = htons(atoi(argv[1]) + 2) ;
    if(bind(download_socket, (struct sockaddr*)&ad_download, sizeof(ad_download)) == -1) {
        perror("Erreur lors du nommage du socket");
        exit(EXIT_FAILURE);
    }
    printf("Socket download nomme\n");

    struct sockaddr_in ad_channel;
    ad_channel.sin_family = AF_INET;
    ad_channel.sin_addr.s_addr = INADDR_ANY ;
    ad_channel.sin_port = htons(atoi(argv[1]) + 3) ;
    if(bind(channel_socket, (struct sockaddr*)&ad_channel, sizeof(ad_channel)) == -1) {
        perror("Erreur lors du nommage du socket");
        exit(EXIT_FAILURE);
    }
    printf("Socket channel nomme\n");

  // Ecoute

    if(listen(dS, 10) == -1) {
        perror("Erreur lors du passage en mode ecoute");
        exit(EXIT_FAILURE);
    }
    printf("Mode ecoute message\n");

    if(listen(upload_socket, 10) == -1) {
        perror("Erreur lors du passage en mode ecoute");
        exit(EXIT_FAILURE);
    }
    printf("Mode ecoute upload\n");

    if(listen(download_socket, 10) == -1) {
        perror("Erreur lors du passage en mode ecoute");
        exit(EXIT_FAILURE);
    }
    printf("Mode ecoute download\n");

    if(listen(channel_socket, 10) == -1) {
        perror("Erreur lors du passage en mode ecoute");
        exit(EXIT_FAILURE);
    }
    printf("Mode ecoute channel\n");

  // We put zeros in the arrays to show that the clients are not connected
  // and that the threads are not created
  memset(tab_client_connecting, 0, sizeof(tab_client_connecting));
  memset(tab_client, 0, sizeof(tab_client));
  memset(Threads_id, 0, sizeof(Threads_id));

  // We set all usernames to an empty string
  int k = 0;
  while (k < MAX_CLIENT) {
    strcpy(tab_username[k], "");
    k = k + 1;
    }

  // We initialise every List of channels for each clients
  // Every client is in the global channel by default
  int j = 0;
  while (j < MAX_CLIENT) {
    tab_channel[j] = new_list();
    add(tab_channel[j], "global");
    j = j + 1;
  }


  // Initialise the semaphores
  sem_init(&free_spot, 0, MAX_CLIENT);
  sem_init(&thread_end, 0, 0);

  // Initialise the mutexes
  pthread_mutex_init(&mutex_tab_client_connecting, NULL);
  pthread_mutex_init(&mutex_tab_username, NULL);
  pthread_mutex_init(&mutex_ended_threads, NULL);
  pthread_mutex_init(&mutex_Threads_id, NULL);
  pthread_mutex_init(&mutex_tab_client, NULL);
  pthread_mutex_init(&mutex_tab_channel, NULL);

  // Initialise the shared queue of disconnected clients
  ended_threads = new_queue();

  // Just in case, we want to know the address of each client
  struct sockaddr_in tab_adr[MAX_CLIENT];
  socklen_t tab_lg[MAX_CLIENT];
  pthread_t tid;
  pthread_t cleanup_tid;

  // Launch the thread that will clean up client threads
  if (pthread_create(&cleanup_tid, NULL, cleanup, NULL) == -1) {
    perror("Erreur lors de la creation du thread");
    exit(EXIT_FAILURE);
  }
  printf("Thread de cleanup cree\n");

  // We intercept the Ctrl+C signal
  signal(SIGINT, handle_interrupt);

  // Acceptation de la connexion des clients
  printf("En attente de connexion des clients\n");

  // We use a variable to know where to put the next client
  // We continually accept connections from clients
  while(1){
    
    // We decrement the semaphore
    sem_wait(&free_spot);

    // We get the first free spot in the array
    int i = get_free_spot();

    // We accept a first connection from a client
    tab_lg[i] = sizeof(struct sockaddr_in);
    tab_client_connecting[i] = accept(dS, (struct sockaddr*) &tab_adr[i],&tab_lg[i]) ;
    if(tab_client_connecting[i] == -1) {
      perror("Erreur lors de la connexion avec le client");
      exit(EXIT_FAILURE);
    }
    printf("Client %d connecte\n", i+1);

    // We create a thread for the client
    // Communication managed by threads
    // Each thread will listen to messages from a client and relay them accordingly
    if (pthread_create(&tid, NULL, client_thread, (void *) &tab_client_connecting[i]) != 0) {
      perror("Erreur lors de la creation du thread");
      exit(EXIT_FAILURE);
    }
    else{
      Threads_id[i] = tid;
    }
    printf("Thread %d cree\n", i+1);

  }

// The server will never reach this point

// Pour etre en accord avec les conventions de la norme POSIX,
// le main doit retourner un int
return 1;
  
}