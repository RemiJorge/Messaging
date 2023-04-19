#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>

// DOCUMENTATION
// This program acts as a server to relay messages between two clients
// It uses the TCP protocol
// It takes one argument, the port to use
// If at some point one of the clients send "fin",
// the server will close the discussion between the clients

// For now, the server can only handle two clients
// If they both disconnect, the server ends its execution

// You can use gcc to compile this program:
// gcc -o serv server.c

// Use : ./serv <port>


// We define our constants
#define MAX_CLIENT 50
#define NB_THREADS 2
#define BUFFER_SIZE 50
// We define an array of socket descriptors for the clients
int tab_client[MAX_CLIENT];  
// We define a variable to know where to put the next client
int* free_spot = tab_client;
int free_indice = 0;


// A function for a thread that will take as an argument
// the socket descriptor of the client, and will receive messages
// from the client and send them to the other clients connected

void * broadcast(void * dS_client) {

    int dSC = *(int *)dS_client;
    char buffer[BUFFER_SIZE];
    int nb_recv;
    int nb_send;
    int i;

    while (1) {

        // We receive the message from the client
        nb_recv = recv(dSC, buffer, BUFFER_SIZE, 0);
        if (nb_recv == -1) {
            perror("Erreur lors de la reception");
            exit(EXIT_FAILURE);
        }
        if (nb_recv == 0) {
            printf("Client déconnecté\n");
            break;
        }

        printf("Message recu: %s du client dSC: %d \n", buffer, dSC);

        // If the client send "fin", we break and close his socket
        if (strcmp(buffer, "fin") == 0) {
            printf("Fin de la discussion pour client dSC: %d\n", dSC);
            break;
        }

        // We have to send the message to the other clients,
        // their socket descriptors are in the array tab_client
        i = 0;
        while( i < MAX_CLIENT){
            // We can't send the message to ourselves
            // also, if a client disconnects, we don't send the message to him
            if (tab_client[i] != 0 && tab_client[i] != dSC) {
                nb_send = send(tab_client[i], buffer, BUFFER_SIZE, 0);
                if (nb_send == -1) {
                    perror("Erreur lors de l'envoi");
                    exit(EXIT_FAILURE);
                }
                // Si jamais un client se deconnecte lorsqu'on est entrain d'envoyer un message aux autres clients
                if (nb_send == 0) {
                    printf("Le client dSC: %d s'est deconnecte,donc le message ne s'est pas envoye a lui\n", tab_client[i]);
                    // Ici, je choisi de ne pas break, 
                    // car je veux quand même envoyer le message aux autres clients
                }
            }
            i = i + 1;
        }
    }

    // We close the socket of the client who wanted to disconnect
    if (close(dSC) == -1) {
        perror("Erreur lors de la fermeture du descripteur de fichier");
        exit(EXIT_FAILURE);
    }

    // We put 0 in the tab_client array
    *(int *)dS_client = 0;
    // We put the adr client in the free_spot variable
    free_spot = dS_client;

    pthread_exit(0);
}




int main(int argc, char *argv[]) {

  if (argc != 2) {
        printf("Error: You must provide exactly 1 argument.\nUsage: ./serv <port>\n");
        exit(EXIT_FAILURE);
    }  

  printf("Debut du Serveur.\n");

  // Creation du socket

  int dS = socket(PF_INET, SOCK_STREAM, 0);
  if(dS == -1) {
    perror("Erreur lors de la création du socket");
    exit(EXIT_FAILURE);
  }
  printf("Socket créé\n");

  // Voici la doc des structs utilises
  /*
  struct sockaddr_in {
    sa_family_t    sin_family;  famille d'adresses : AF_INET     
    uint16_t       sin_port;    port dans l'ordre d'octets réseau
    struct in_addr sin_addr;    adresse Internet                 
  };

 Adresse Internet 
  struct in_addr {
    uint32_t    s_addr;   Adresse dans l'ordre d'octets réseau 
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
  printf("Socket nommé\n");

  // Ecoute

  if(listen(dS, 10) == -1) {
    perror("Erreur lors du passage en mode écoute");
    exit(EXIT_FAILURE);
  }
  printf("Mode écoute\n");

  // We put zeros in the array to show that the clients are not connected
  memset(tab_client, 0, sizeof(tab_client));

  // Just in case we want to know the address of the clients
  struct sockaddr_in tab_adr[MAX_CLIENT];
  socklen_t tab_lg[MAX_CLIENT];
  pthread_t tid;
  pthread_t Threads_id [MAX_CLIENT] ;

  // Acceptation de la connexion des deux clients
  printf("En attente de connexion des clients\n");

  // We use a variable to know where to put the next client
  // We continually accept connections from clients
  while(1){
    
    int i = free_indice;
    tab_lg[i] = sizeof(struct sockaddr_in);
    tab_client[i] = accept(dS, (struct sockaddr*) &tab_adr[i],&tab_lg[i]) ;
    if(tab_client[i] == -1) {
      perror("Erreur lors de la connexion avec le client");
      exit(EXIT_FAILURE);
    }
    printf("Client %d connecté\n", i+1);
  
    // Communication managed by threads
    // Each thread will listen to messages from a client and send 
    // the messages to the rest of the clients (broadcast)
    if (pthread_create(&tid, NULL, broadcast, (void *) &tab_client[i]) != 0) {
      perror("Erreur lors de la création du thread");
      exit(EXIT_FAILURE);
    }
    else{
      Threads_id[i] = tid;
    }

    // We find the first occurence of 0 in the array
    // If there is no 0, we will be out of the loop
    int j = 0;
    while (j < MAX_CLIENT){
      if (tab_client[j] == 0){
        free_indice = j;
        break;
      }
      j = j + 1;
    }
  
  }
  // We wait for the threads to finish
  for (int i = 0; i < MAX_CLIENT; i++){
    if (pthread_join(Threads_id[i], NULL) != 0) {
      perror("Erreur lors de l'attente de la fin du thread");
      exit(EXIT_FAILURE);
    }
  }

  printf("Both clients disconnected\n");

  // Close the main socket
  if (close(dS)==-1){
    perror("Erreur lors de la fermeture du socket de acceptation");
    exit(EXIT_FAILURE);
  }

// Pour etre en accord avec les conventions de la norme POSIX,
// le main doit retourner un int
return 1;
  
}