# Message Relay Server Between Two Clients

**VERY VERY IMPORTANT:**

**PLEASE READ THE USER GUIDE TO LEARN HOW TO USE THE MESSAGING SYSTEM.**

**PLEASE READ THE DOCUMENTATION FOR MORE INFORMATION.**

This is a simple messaging server written in C that relays text messages
between multiple clients using the TCP protocol. The server and the clients are multi-threaded.

The server is able to handle multiple clients.

You can send and receive files to and from the server!

You can now join, leave, create and delete channels!

We have improved the graphical interface of the client
Please enjoy the new features!

## Compilation

To compile the server and the client, run the following command:
./compil.sh

## IMPORTANT:

**BEFORE EXECUTION, MAKE SURE YOU ARE IN THE BIN FOLDER**

Launch the server first.

=> ./server port

Then the clients

=> ./client ip port 


## Commands

Voici le guide d'utilisation de la messagerie:

@pseudo message

Mentionne une personne specifique sur le server, affiche le message en evidence

@everyone message

Mentionne toutes les personnes actuellement sur le server

/fin

Permet de mettre fin au protocole de communication et fermer le programme

/mp pseudo message

Envoie un message privé à la personne mentionnée par le pseudo

/man

Affiche le guide d'utilisation

/list 

Affiche tous les utilisateurs connectés

/who

Renvoie le pseudo

/upload fichier

Telecharge  le fichier qui se trouve dans le repertoire de client_files vers le server

/upload 

Ouvre le menu de selection de fichier afin d'envoyer un fichier de client_files vers le server

/download

Ouvre le menu de selection de fichier afin de télecharger le fichier choisi depuis le server 

/salon
Ouvre le menu des salons pour pouvoir creer, rejoindre, quitter et supprimer des salons 

/exit
Commande a taper dans un salon. Permet de quitter le salon. La fenetre du salon se fermera automatiquement.


## File Architecture

This is how the files are organized in this repository
```bash
.
├── bin
│   ├── client
│   ├── client_salon
│   └── server
├── compil.sh
├── README.md
└── src
    ├── client.c
    ├── client_files
    │   ├── alex.txt
    │   ├── document.txt
    │   ├── elgreco.jpg
    │   ├── image.jpg
    │   ├── linux.png
    │   ├── Nature_.jpg
    │   └── nyan.gif
    ├── client_salon.c
    ├── manuel.txt
    ├── server.c
    ├── server_channels
    │   ├── alex
    │   ├── cours
    │   ├── game
    │   ├── music
    │   ├── poke
    │   ├── school
    │   └── swift
    └── server_files
        ├── alex.txt
        ├── document.txt
        ├── elgreco.jpg
        ├── image.jpg
        ├── linux.png
        ├── Nature_.jpg
        └── nyan.gif

5 directories, 30 files
```
