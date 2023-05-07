#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <semaphore.h>

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
#define MSG_SIZE 970
// Size of color
#define COLOR_SIZE 10
// Buffer size for messages (this is the total size of the message)
#define BUFFER_SIZE USERNAME_SIZE + CMD_SIZE + MSG_SIZE + COLOR_SIZE

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
    // Possible commands : "dm", "who", "fin", "list"
    char cmd[CMD_SIZE];
    // If the server is receiving the message:
        // If the command is "dm", the username of the client to send the message to
        // If the command is "who", username is empty
        // If the command is "fin", username is empty
        // If the command is "list", username is empty
        // If the client is trying to connect, username is the username of the client
    // If the server is sending the message:
        // If the command is "dm", username is who sent the message
        // If the command is "who", username is Server
        // If the command is "list", username is Server
    char username[USERNAME_SIZE];
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
    strcpy(buffer->username, tab_username[client_indice]);
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
        if (get_indice_username(buffer->username) == -1) {
            // We put the client in the tab_client array
            // Lock the mutex because we are going to write in the tab_client array
            pthread_mutex_lock(&mutex_tab_client);
            tab_client[client_indice_connecting] = dSC_connection;
            // Unlock the mutex
            pthread_mutex_unlock(&mutex_tab_client);
            // We put the username in the tab_username array
            // Lock the mutex because we are going to write in the tab_username array
            pthread_mutex_lock(&mutex_tab_username);
            strcpy(tab_username[client_indice_connecting], buffer->username);
            // Unlock the mutex
            pthread_mutex_unlock(&mutex_tab_username);
            // We send true to the client
            strcpy(buffer->username, "Server");
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
            strcpy(buffer->username, "Server");
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

    // We tell the other clients that a new client has connected
    // Lock the mutex
    pthread_mutex_lock(&mutex_tab_username);
    strcpy(buffer->username, tab_username[client_indice]);
    // Unlock the mutex
    pthread_mutex_unlock(&mutex_tab_username);
    strcpy(buffer->message, "Je me connecte. Bonjour!");
    send_to_all(client_indice, buffer);

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
            strcpy(buffer->username, "Serveur");
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
            strcpy(buffer->username, "Serveur");
            strcpy(buffer->cmd, "who");
            // Lock the mutex
            pthread_mutex_lock(&mutex_tab_username);
            strcpy(buffer->message, tab_username[client_indice]);
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
            int client_to_send = get_indice_username(buffer->username);
            // If the client is not in the array, we send an error message to the client
            if (client_to_send == -1) {
                strcpy(buffer->username, "Serveur");
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
            pthread_mutex_lock(&mutex_tab_username);
            strcpy(buffer->username, tab_username[client_indice]);
            nb_send = send(tab_client[client_to_send], buffer, BUFFER_SIZE, 0);
            // Unlock the mutex
            pthread_mutex_unlock(&mutex_tab_username);
            if (nb_send == -1) {
                perror("Erreur lors de l'envoi");
                printf("L'erreur est dans le thread du client: %d\n", client_indice + 1);
                exit(EXIT_FAILURE);
            }
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
    enqueue(disconnected_clients, client_indice);
    // Unlock the mutex
    pthread_mutex_unlock(&mutex_disconnected_clients);

    // We increment the semaphore for thread cleanup
    sem_post(&thread_end);
    // We increment the semaphore to indicate that there is a free spot in the tab_client array
    sem_post(&free_spot);

    pthread_exit(0);
}


/***************************************
        Thread Cleanup Handler
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
  printf("Socket nomme\n");

  // Ecoute

  if(listen(dS, 10) == -1) {
    perror("Erreur lors du passage en mode ecoute");
    exit(EXIT_FAILURE);
  }
  printf("Mode ecoute\n");

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

  // Initialise the shared queue of disconnected clients
  disconnected_clients = new_queue();

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

  // Acceptation de la connexion des clients
  printf("En attente de connexion des clients\n");

  // We use a variable to know where to put the next client
  // We continually accept connections from clients
  while(1){
    
    // We decrement the semaphore
    sem_wait(&free_spot);

    // We get the first free spot in the array
    int i = get_free_spot();

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