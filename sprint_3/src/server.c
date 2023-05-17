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

// DOCUMENTATION
// This program acts as a server to relay messages between multiple clients
// It uses the TCP protocol
// It takes one argument, the port to use

// Please read the README.md file for more information
// including the different commands that can be used

// You can use gcc to compile this program:
// gcc -o serv server.c

// Use : ./serv <port>


/*****************************************************
              Queue Type Def and Functions
******************************************************/

typedef struct Queue Queue;
typedef struct Element Element;

struct Element{
    int number;
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
void enqueue(Queue * q, int number){
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
int dequeue(Queue * q){
    if(q->premier == NULL){
        return -1;
    }else{
        Element * e = q->premier;
        q->premier = e->next;
        int number = e->number;
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

// Maximum number of clients that can connect to the server
#define MAX_CLIENT 25
// Username size
#define USERNAME_SIZE 10
// Size of commands
#define CMD_SIZE 10
// Size of the message
#define MSG_SIZE 400
// Size of color
#define COLOR_SIZE 10
// Buffer size for messages (this is the total size of the message)
#define BUFFER_SIZE USERNAME_SIZE + USERNAME_SIZE + CMD_SIZE + MSG_SIZE + COLOR_SIZE

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

// A semaphore to indicate the number of free spots in the tab_client array
sem_t free_spot;

// The socket descriptor for the socket that deals with file uploads
int upload_socket;

// Mutex to protect the upload_socket
pthread_mutex_t mutex_upload_socket;

// The socket descriptor for the socket that deals with file downloads
int download_socket;

// Mutex to protect the download_socket
pthread_mutex_t mutex_download_socket;


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
Queue * disconnected_clients;

// Mutex to protect the disconnected_clients queue
pthread_mutex_t mutex_disconnected_clients;

// A shared queue to store the ids of threads that have ended for upload and download
Queue * thread_queue;

// Mutex to protect the thread_queue queue
pthread_mutex_t mutex_thread_queue;

// A semaphore to indicate when a thread has ended for upload and download
sem_t thread_end_upload_download;

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
    // Possible commands : "dm", "who", "fin", "list", "upload", "download"
    char cmd[CMD_SIZE];
    // The username of the client who will receive the message
    // It can be Server if the message is sent from the server
    char from[USERNAME_SIZE];
    // The username of the client who sent the message
    // It can be Server if the message is sent to the server
    char to[USERNAME_SIZE];
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
    // Lock the mutex
    pthread_mutex_lock(&mutex_tab_username);
    strcpy(buffer->from, tab_username[client_indice]);
    // Unlock the mutex
    pthread_mutex_unlock(&mutex_tab_username);
    // Lock the mutex
    pthread_mutex_lock(&mutex_tab_client);
    while( i < MAX_CLIENT){
        // We can't send the message to ourselves
        // also, if a client disconnects, we don't send the message to him
        if (tab_client[i] != 0 && i != client_indice) {
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
        i = i + 1;
    }
    // Unlock the mutex
    pthread_mutex_unlock(&mutex_tab_client);
}


// A function for a thread that will accept a connection using the socket 
// for uploads and will create and receive the file and write in the file

void * upload_file_thread(void * arg){
    int nb_recv; // The number of bytes received
    int nb_send; // The number of bytes sent
    Message msg_buffer; // The buffer for the messages
    Message * buffer = &msg_buffer; // A pointer to the buffer
    int dS_thread_upload; // The socket for the upload for the accept

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
        // We close the socket
        close(dS_thread_upload);
        // Push thread id to queue and increment semaphore
        //TODO
        // We exit the thread
        pthread_exit(NULL);
    }
    printf("Le nom du fichier est: %s\n", buffer->message);


    // We create the file
    char path[MSG_SIZE];
    strcpy(path, "server_files/");
    strcat(path, buffer->message);
    FILE *file = fopen(path, "w");
    if (file == NULL) {
        perror("Erreur lors de la creation du fichier");
        //TODO: close socket and push id to queue
        exit(EXIT_FAILURE);
    }
    printf("Le fichier %s a ete cree\n", buffer->message);

    // We receive the size of the file
    long file_size;
    nb_recv = recv(dS_thread_upload, &file_size, sizeof(long), 0);
    if (nb_recv == -1) {
        perror("Erreur lors de la reception upload client");
        exit(EXIT_FAILURE);
    }
    // If ever a client disconnect while we are receiving the messages
    if (nb_recv == 0) {
        printf("Le client s'est deconnecte dans le file upload\n");
        // We close the socket
        close(dS_thread_upload);
        // Push thread id to queue and increment semaphore
        //TODO
        // We exit the thread
        pthread_exit(NULL);
    }
    printf("La taille du fichier est: %ld\n", file_size);
    
    // Packet for the file data
    char packet[BUFFER_SIZE];
    int nb_recv_total = 0;

    while(1){
        nb_recv = recv(dS_thread_upload, packet, BUFFER_SIZE, 0);
        printf("nb_recv: %d\n", nb_recv);
        if (nb_recv == -1) {
            perror("Erreur lors de la reception");
            exit(EXIT_FAILURE);
        }
        // If ever a client disconnect while we are receiving the messages
        if (nb_recv == 0) {
            printf("Le client s'est deconnecte lors de upload de file\n");
            break;
        }
        nb_recv_total = nb_recv_total + nb_recv;
        printf("nb_recv_total: %d\n", nb_recv_total);

        // If the nb_recv_total is equal to the file_size, we stop the loop
        /*
        if (nb_recv_total >= file_size) {
            break;
        }
        */
        printf("message: %s\n", packet);
        
        // We write in the file
        fwrite(packet, sizeof(char), nb_recv, file);
        printf("writing in the file\n");
    }
    fclose(file);
    printf("Le fichier a ete ferme\n");
    
    /*
    // We put the ThreadId in the queue

    printf("avant lock upload %ld\n", ThreadId);
    // Lock the mutex
    pthread_mutex_lock(&mutex_thread_queue);
    enqueue(thread_queue, (unsigned long int) ThreadId);
    // Unlock the mutex
    pthread_mutex_unlock(&mutex_thread_queue);

    // We increment the semaphore
    sem_post(&thread_end_upload_download);
    printf("avant quitte upload\n");
    */

    // We exit the thread
    pthread_exit(0);
}


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
            strcpy(buffer->message, "Je me deconnecte. Au revoir!");
            // We send a message to the other clients to tell them that this client has disconnected
            send_to_all(client_indice, buffer);
            break;
        }

        printf("Message recu: %s du client: %d \n", buffer->message, client_indice + 1);

        // If the client sends "fin", we break and close his socket
        if (strcmp(buffer->cmd, "fin") == 0) {
            printf("Fin de la discussion pour client: %d\n", client_indice + 1);
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

        // If the client sends "dm", we send the message to the person who's username is in buffer.username
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

        printf("TTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTT\n");
        // If the client sends "upload", we create a file in the /server_files directory
        // named after the name in buffer->message
        // Then we launch a thread to receive the file
        if (strcmp(buffer->cmd, "upload") == 0) {
            printf("UPLOAD detected\n");

            // We launch a thread to receive the file
            pthread_t thread_upload;

            if (pthread_create(&thread_upload, NULL, upload_file_thread, NULL) != 0) {
                perror("Erreur lors de la creation du thread");
                printf("L'erreur est dans le thread du client: %d\n", client_indice + 1);
                exit(EXIT_FAILURE);
            }
            printf("THREAD UPLOAD CREE\n");
           
            // We send a message to the other clients to tell them that this client has uploaded a file
            strcpy(buffer->cmd, "upload");
            strcpy(buffer->message, "I am uploading a file!");
            send_to_all(client_indice, buffer);
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

    // We put the thread index in the shared queue of disconnected clients
    // Lock the mutex
    pthread_mutex_lock(&mutex_disconnected_clients);
    enqueue(disconnected_clients, (unsigned long int) client_indice);
    // Unlock the mutex
    pthread_mutex_unlock(&mutex_disconnected_clients);

    // We increment the semaphore for thread cleanup
    sem_post(&thread_end);
    // We increment the semaphore to indicate that there is a free spot in the tab_client array
    sem_post(&free_spot);

    pthread_exit(0);
}








/***************************************
        Thread Cleanup Handlers
****************************************/

// A function for a thread that will cleanup client threads
// The thread that cleans up sleeps until a client thread ends
// It checks a semaphore to see if a client thread has ended
// If a client thread has ended, it joins the thread and cleans up the thread
// It gets the index of the client thread that ended from the shared queue of disconnected clients

void * cleanup(void * arg) {

    while (1) {
        sem_wait(&thread_end);
        int thread_index;
        // We get the index of the client thread that ended from the shared queue of disconnected clients
        // Lock the mutex
        pthread_mutex_lock(&mutex_disconnected_clients);
        thread_index = dequeue(disconnected_clients);
        // Unlock the mutex
        pthread_mutex_unlock(&mutex_disconnected_clients);
        if (thread_index == -1) {
            perror("ERREUR CRITIQUE DEQUEUE");
            exit(EXIT_FAILURE);
        }
        // We join the thread
        // Lock the mutex
        pthread_mutex_lock(&mutex_Threads_id);
        printf("Thread %ld joined\n", Threads_id[thread_index]);
        if (pthread_join(Threads_id[thread_index], NULL) == -1){
            perror("Erreur lors du join d'un thread");
        }
        else{
            printf("Thread %d joined\n", thread_index + 1);
            Threads_id[thread_index] = 0;
        }
        // Unlock the mutex
        pthread_mutex_unlock(&mutex_Threads_id);
    }

    pthread_exit(0);
}


// A function for a thread that will clean up the upload and download threads
// The thread that cleans up sleeps until a upload or download thread ends
// It checks a semaphore to see if a upload or download thread has ended
// If a upload or download thread has ended, it joins the thread and cleans up the thread
// It gets the id of the upload or download thread that ended from the shared queue of ended upload or download threads

void * cleanup_upload_download(void * arg) {

    while (1) {
        printf("AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\n");
        sem_wait(&thread_end_upload_download);
        pthread_t thread_id;
        // We get the id of the upload or download thread that ended from the shared queue of ended upload or download threads
        // Lock the mutex
        printf("BBBBBBBBBBBBBBBBBBBb\n");
        pthread_mutex_lock(&mutex_thread_queue);
        thread_id = (pthread_t) dequeue(thread_queue);
        // Unlock the mutex
        pthread_mutex_unlock(&mutex_thread_queue);
        printf("CCCCCCCCCCCCCCC\n");
        if (thread_id == -1) {
            perror("ERREUR CRITIQUE DEQUEUE");
            exit(EXIT_FAILURE);
        }
        // We join the thread
        /*
        printf("DDDDDDDDDDDDDDDDD\n");
        printf("thread_id = %d\n", thread_id);
        if (pthread_join(thread_id, NULL) == -1){
            perror("Erreur lors du join d'un thread");
        }
        else{
            printf("Thread upload %d joined\n", thread_id);
        }
        */
        printf("join thread simul\n");
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

  // Creation du socket

  int dS = socket(PF_INET, SOCK_STREAM, 0);
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


  // Initialise the semaphores
  sem_init(&free_spot, 0, MAX_CLIENT);
  sem_init(&thread_end, 0, 0);
  sem_init(&thread_end_upload_download, 0, 0);

  // Initialise the mutexes
  pthread_mutex_init(&mutex_tab_client_connecting, NULL);
  pthread_mutex_init(&mutex_tab_username, NULL);
  pthread_mutex_init(&mutex_disconnected_clients, NULL);
  pthread_mutex_init(&mutex_Threads_id, NULL);
  pthread_mutex_init(&mutex_thread_queue, NULL);

  // Initialise the shared queue of disconnected clients
  disconnected_clients = new_queue();
  thread_queue = new_queue();

  // Just in case, we want to know the address of each client
  struct sockaddr_in tab_adr[MAX_CLIENT];
  socklen_t tab_lg[MAX_CLIENT];
  pthread_t tid;
  pthread_t cleanup_tid;
  pthread_t cleanup_upload_download_tid;

  // Launch the thread that will clean up client threads
  if (pthread_create(&cleanup_tid, NULL, cleanup, NULL) == -1) {
    perror("Erreur lors de la creation du thread");
    exit(EXIT_FAILURE);
  }
  printf("Thread de cleanup cree\n");

  // Launch the thread that will cleanup upload and download threads
    if (pthread_create(&cleanup_upload_download_tid, NULL, cleanup_upload_download, NULL) == -1) {
        perror("Erreur lors de la creation du thread");
        exit(EXIT_FAILURE);
    }
    printf("Thread de cleanup upload et download cree\n");

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
  
  // This part is never reached

  // We join all the threads in Threads_id that are not 0
  int i = 0;
  while (i < MAX_CLIENT) {
    if (Threads_id[i] != 0) {
      if (pthread_join(Threads_id[i], NULL) == -1){
        perror("Erreur lors du join d'un thread");
      }
      else{
        printf("Thread %d joined\n", i);
      }
    }
    i = i + 1;
  }

  //Then we join the cleanup thread
  if (pthread_join(cleanup_tid, NULL) == -1){
    perror("Erreur lors du join du thread de cleanup");
  }
  else{
    printf("Thread de cleanup joined\n");
  }

  // Close the main socket
  if (close(dS)==-1){
    perror("Erreur lors de la fermeture du socket de acceptation");
    exit(EXIT_FAILURE);
  }

// Pour etre en accord avec les conventions de la norme POSIX,
// le main doit retourner un int
return 1;
  
}