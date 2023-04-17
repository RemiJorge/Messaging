#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

// DOCUMENTATION
// This program acts as a server to relay messages between two clients
// It uses the TCP protocol
// It takes one argument, the port to use
// If at some point one of the clients send "fin",
// the server will close the discussion between the clients
// and wait for a new one.

// You can use gcc to compile this program:
// gcc -o serv server.c

// Use : ./serv <port>

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


while (1){
  
  // Acceptation de la connexion des deux clients
  printf("En attente de connexion des clients\n");

  struct sockaddr_in adrC_1 ;
  socklen_t lg_1 = sizeof(struct sockaddr_in) ;
  int dSC_1 = accept(dS, (struct sockaddr*) &adrC_1,&lg_1) ;
  if(dSC_1 == -1) {
    perror("Erreur lors de la connexion avec le client");
    exit(EXIT_FAILURE);
  }
  printf("Client 1 connecté\n");

  struct sockaddr_in adrC_2 ;
  socklen_t lg_2 = sizeof(struct sockaddr_in) ;
  int dSC_2 = accept(dS, (struct sockaddr*) &adrC_2,&lg_2) ;
  if(dSC_2 == -1) {
    perror("Erreur lors de la connexion avec le client");
    exit(EXIT_FAILURE);
  }
  printf("Client 2 connecté\n");

while (1){

  // Communication, ici:
  // Client 1 : write puis read
  // Client 2 : read puis write

  char msg [50] ;
  int nb_recv;
  int nb_sent;

  // Client 1 envoie un message

  nb_recv = recv(dSC_1, msg, sizeof(msg), 0);
  if (nb_recv == -1) {
      perror("Erreur lors de la réception du message");
      break;
  } 
  else if (nb_recv == 0) {
      // Remote endpoint has closed the connection
      printf("Connection closed by remote endpoint\n");
      break;
  }

  // Data has been received
  printf("Message reçu : %s\n", msg);
  printf("Nb octects recus : %d\n", nb_recv);

  // Check if client 1 wants to end the discussion

  if (strcmp(msg, "fin") == 0) {
    printf("Client 1 met fin a la discussion\n");
    // On dit au client 2 que le client 1 a fini
    nb_sent = send(dSC_2, msg, sizeof(msg), 0);
    if (nb_sent == -1) {
        perror("Erreur lors de l'envoi du message");
    }
    break;
  }

  // On envoie le message au client 2

  nb_sent = send(dSC_2, msg, sizeof(msg), 0);
  if (nb_sent == -1) {
      perror("Erreur lors de l'envoi du message");
      break;
  }
  else if(nb_sent == 0) {
      // Remote endpoint has closed the connection
      printf("Connection closed by remote endpoint\n");
      break;
  }
  printf("Message envoyé : %s\n", msg);
  printf("Nb octects envoyés : %d\n", nb_sent);

  // Client 2 envoie un message

  nb_recv = recv(dSC_2, msg, sizeof(msg), 0);
  if (nb_recv == -1) {
      perror("Erreur lors de la réception du message");
      break;
  } 
  else if (nb_recv == 0) {
      // Remote endpoint has closed the connection
      printf("Connection closed by remote endpoint\n");
      break;
  }

  // Data has been received
  printf("Message reçu : %s\n", msg);
  printf("Nb octects recus : %d\n", nb_recv);

  // Check if client 2 wants to end the discussion

  if (strcmp(msg, "fin") == 0) {
    printf("Client 2 met fin a la discussion\n");
    // On dit au client 1 que le client 2 a fini
    nb_sent = send(dSC_1, msg, sizeof(msg), 0);
    if (nb_sent == -1) {
      perror("Erreur lors de l'envoi du message");
    }
    break;
  }

  // On envoie le message au client 1

  nb_sent = send(dSC_1, msg, sizeof(msg), 0);
  if (nb_sent == -1) {
      perror("Erreur lors de l'envoi du message");
      break;
  }
  else if(nb_sent == 0) {
      // Remote endpoint has closed the connection
      printf("Connection closed by remote endpoint\n");
      break;
  }
  printf("Message envoyé : %s\n", msg);
  printf("Nb octects envoyés : %d\n", nb_sent);

}

  // Fermeture de la discussion entre les deux clients

  if (close(dSC_1) == -1){
    perror("Erreur de close du socket");
    exit(EXIT_FAILURE);
  }

  if (close(dSC_2) == -1){
    perror("Erreur de close du socket");
    exit(EXIT_FAILURE);
  }

}

// Pour etre en accord avec les conventions de la norme POSIX,
// le main doit retourner un int
return 1;
  
}