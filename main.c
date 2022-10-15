#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

#define CLIENT_ADDR "127.0.0.1" // adresse client
#define SRV_PORT 3550 // Port when server launched first
#define CLT_PORT 8889 // Port when connected last

// Standard definitions
#define BOARD_SIZE 10
#define S_PERDU -1 // Partie perdue
#define S_TOUCHE 1 // Bateau touché
#define S_MANQUE 2 // Bateau manqué
#define S_TOUCHE_BIS 3 // Bateau touché
#define S_COULE 4 //Bateau coulé.
#define S_HANDSHAKE 10 // Code de poignée de main

// les différents types de bateaux
//1 bateau d’une case, un bateau de 2 cases, un bateau de 3 cases

#define B_PORTE_AVION 1
#define B_SOUS_MARIN 3
#define B_TORPILLEUR 2

// les valeurs des cellules
#define C_EAU 0 // eau
#define C_EAU_T 1 // eau touché
#define C_BAT_T 2
#define C_PORTE_AVION 10
#define C_PORTE_AVION_T 11
#define C_SOUS_MARIN 40
#define C_SOUS_MARIN_T 41
#define C_TORPILLEUR 50
#define C_TORPILLEUR_T 51

// Couleurs
#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_RESET   "\x1b[0m"

// Les options de rendu de la grille
const char line[] = "  +---+---+---+---+---+---+---+---+---+---+",
				EAU[] = "   |",
				EAU_T[] = ANSI_COLOR_BLUE " X " ANSI_COLOR_RESET "|",
				BAT[] = ANSI_COLOR_GREEN "***" ANSI_COLOR_RESET "|",
				BAT_T[] = ANSI_COLOR_RED "XXX" ANSI_COLOR_RESET "|";

typedef struct {
	int socket_desc;
	int client_sock;
	int c;
	int read_size;
	struct sockaddr_in server;
	struct sockaddr_in client;
	char client_message[2000];

} InfosServer;

typedef struct {
	/* 1 porte-avion (1 cases),
	 * 1 sous-marin (3 cases),
	 * 1 torpilleur (2 cases),
	 */
	// Cases restantes pour les bateaux
	int porteAvion, contreTorpilleur, sousMarin, torpilleur, points;
	// int croiseur;
	int grille[BOARD_SIZE][BOARD_SIZE];
	int grilleEnnemie[BOARD_SIZE][BOARD_SIZE];
} Plateau;

typedef struct {
	int x, y;
	char d;
} Coordonnees;

Coordonnees strToCoord(char string[], int hasDirection) {
	Coordonnees c;
	char strX[2 + 1];
	int i;

	//les directions du bateaux
	if (hasDirection > 0) {
		hasDirection = 1;
		c.d = string[strlen(string) - 1];
	} else {
		hasDirection = 0;
	}

// les diviseurs de chaine
	c.y = string[0] - 'a';
	//	printf("Debug strX- lg: %i, iMax: %i, str: %s ", (int) strlen(string), (int) strlen(string)-(1 + hasDirection), string);
	for (i = 0; i < strlen(string)-(1 + hasDirection); i++) {
		strX[i] = string[i + 1];
	}

	for (i++; i < strlen(strX); i++) {
		strX[i] = '\0';
	}
	//	printf("strX: %s\n", strX);

	c.x = strtol(strX, NULL, 10) - 1;

	return c;
}

 // Initialise un plateau vierge

Plateau initPlateau() {
	Plateau p;
	int i, j;

	p.porteAvion = B_PORTE_AVION;
	p.sousMarin = B_SOUS_MARIN;
	p.torpilleur = B_TORPILLEUR;
	p.points = 0;

	for (i = 0; i < BOARD_SIZE; i++) {
		for (j = 0; j < BOARD_SIZE; j++) {
			p.grille[i][j] = C_EAU;
			p.grilleEnnemie[i][j] = C_EAU;
		}
	}

	return p;
}

// Affiche une grille de jeu

void afficherGrille(int g[BOARD_SIZE][BOARD_SIZE]) {
	int i, j;

	/*
	| 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10|
	+---+---+---+---+---+---+---+---+---+---+
A |   |   |   |   |   |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+---+---+
B |   |   |   |   |   |   |   |   |   |   |
	+---+---+---+---+---+---+---+---+---+---+
... J
	 */
	puts("\n  | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10|");
	puts(line);
	for (i = 0; i < BOARD_SIZE; i++) {
		printf("%c |", i + 'A');
		for (j = 0; j < BOARD_SIZE; j++) {
			if ((int) g[i][j] == C_EAU) { // Dans l'eau
				printf("%s", EAU);
			} else if (g[i][j] == C_EAU_T) { // Eau, touché
				printf("%s", EAU_T);
			} else if (// Bateau
							g[i][j] == C_PORTE_AVION ||
							g[i][j] == C_SOUS_MARIN ||
							g[i][j] == C_TORPILLEUR
							) {
				printf("%s", BAT);
			} else {
				printf("%s", BAT_T);
			}
		}
		printf("\n");
		puts(line);
	}
}

// Retourne le nombre de cellules encore vivantes

int calcAlive(Plateau p) {
	return p.contreTorpilleur  + p.porteAvion + p.sousMarin + p.torpilleur; // + croiseur
}


// placer un bateau dans la grille

Plateau placeShip(Plateau p, char nom[], int size, int val) {
	int done, error, i;
	Coordonnees c;
	char pos[4 + 1], // Coordonnées
					orientation[10 + 1],
					reponse;

	afficherGrille(p.grille);
	// L'explication du jeu a10v

	printf("Pour placer un bateau, donnez l'adresse de la case de destination, puis\n"
					"son orientation (h/v). Les batiments seront positionnés sur la droite de\n"
					"la case donnée pour les placements horizontaux, et vers le base pour les \n"
					"placements verticaux. Exemple : a10v\n\n");
	printf("Veuillez placer le %s (%i cases)\n\n", nom, size);
	do {
  //réinitialiser les variables
		error = 0;
		done = 0;
		strcpy(orientation, "horizontal");

		// Demande de confirmation de la position d'un bateau
		printf("Position : ");
		scanf("%s", pos);

		c = strToCoord(pos, 1);

		//		printf("x: %i, y: %i, o: %c\n", c.x, c.y, c.d);

		if (c.x < 0 || c.y < 0 || c.x > BOARD_SIZE || c.y > BOARD_SIZE) {
			puts(" > Mauvaises coordonnées...");
			error = 1;
		} else if (c.d == 'v') {// Vérification placement des bateaux
			strcpy(orientation, "vertical");
			// Sortie de carte
			if (c.y + size > BOARD_SIZE) {
				printf(" > Vous ne pouvez pas placer votre bateau ici. Il sortirait de la carte...(y=%i)\n", c.y);
				error = 1;
			} else {
				// Chevauchements
				for (i = c.y; i < c.y + size; i++) {
					if (p.grille[i][c.x] != C_EAU) {
						puts(" > Il y a déjà un bateau ici...");
						error = 1;

						break;
					}
				}
			}
		} else if (c.x + size > BOARD_SIZE) {
			printf(" > Vous ne pouvez pas placer votre bateau ici. Il sortirait de la carte...(x=%i)\n", c.x);
			error = 1;
		} else {
			// Chevauchements
			for (i = c.x; i < c.x + size; i++) {
				if (p.grille[c.y][i] != C_EAU) {
					puts(" > Il y a déjà un bateau ici...");
					error = 1;

					break;
				}
			}
		}

		if (error == 0) {
			getchar();
			printf("Placement %s en %c:%i. Est-ce correct ? [o/N] ", orientation, c.y + 'a', c.x + 1);
			reponse = getchar();
			if (reponse == 'o' || reponse == 'O') {
				done = 1;
			}
		}
	} while (done == 0);

	// Placer un bateau dans la grille
	if (c.d == 'v') {
		for (i = c.y; i < c.y + size; i++) {
			p.grille[i][c.x] = val;
		}
	} else {
		for (i = c.x; i < c.x + size; i++) {
			p.grille[c.y][i] = val;
		}
	}
	return p;
}


void closeApp(InfosServer is, int code, char message[50]) {
	// Fermets les sockets
	close(is.socket_desc);
	close(is.client_sock);

	// Présentation d'un message
	printf("\n%s\n", message);

	// Sortie
	exit(code);
}


 // Initialiser le serveur : ecoute du port

  InfosServer initServer(int port) {
	InfosServer is;

	// Creation du socket
	is.socket_desc = socket(AF_INET, SOCK_STREAM, 0);
	if (is.socket_desc == -1) {
		closeApp(is, 1, "Impossible de créer le socket.");
	}

	// Création de l'écoute
	is.server.sin_family = AF_INET;
	is.server.sin_addr.s_addr = INADDR_ANY;
	is.server.sin_port = htons(port); // définition du port d'écoute du socket

	// Mise en place de l'écoute
	if (bind(is.socket_desc, (struct sockaddr *) &is.server, sizeof (is.server)) < 0) {
		closeApp(is, 1, "Erreur de la mise en place de l'écoute.");
	}

	listen(is.socket_desc, 3);

	printf("Serveur en place (port %i). En attente de client...\n", port);
	is.c = sizeof (struct sockaddr_in);

	return is;
}


  //Ecoute sur le port du serveur pour un message entrant.

int receptionMessageClient(InfosServer is, Plateau * p) {
	char server_response[20];
	int content, status, pv = -1;
	Coordonnees coup;

	// Acceptation de la connexion d'un client
	is.client_sock = accept(is.socket_desc, (struct sockaddr *) &is.client, (socklen_t*) & is.c);
	if (is.client_sock < 0) {
		perror("Connexion entrante refusee");
		return 1;
	}
	//	puts("Connexion entrante acceptée");

	/*
	 * "Interface graphique
	 */
	printf("\n-------------------------------------\n");
	printf("En attente du coup de l'adversaire...\n");

	/*
	 * Reception d'un message par le client
	 */
	while ((is.read_size = recv(is.client_sock, is.client_message, 100, 0)) > 0) {
		// Affichage du message
		printf(ANSI_COLOR_BLUE " > %s\n\n" ANSI_COLOR_RESET, is.client_message);

		// Traitement
		coup = strToCoord(is.client_message, 0);
		content = (*p).grille[coup.y][coup.x];

		if (content == C_EAU || content == C_EAU_T) {
			sprintf(server_response, "%d", S_MANQUE);
			(*p).grille[coup.y][coup.x] = C_EAU_T;
			status = S_MANQUE;
		} else {
			status = S_TOUCHE;
			sprintf(server_response, "%d", S_TOUCHE);
			switch (content) {
				case C_PORTE_AVION:
					(*p).grille[coup.y][coup.x] = C_PORTE_AVION_T;
					(*p).porteAvion--;
					pv = (*p).porteAvion;
					break;
				case C_SOUS_MARIN:
					(*p).grille[coup.y][coup.x] = C_SOUS_MARIN_T;
					(*p).sousMarin--;
					pv = (*p).sousMarin;
					break;
				case C_TORPILLEUR:
					(*p).grille[coup.y][coup.x] = C_TORPILLEUR_T;
					(*p).torpilleur--;
					pv = (*p).torpilleur;
					break;
				default: // Bateau déjà touché
					sprintf(server_response, "%d", S_TOUCHE_BIS);
					status = S_MANQUE;
			}
		}

		if (pv == 0) {
			status = S_TOUCHE;
			sprintf(server_response, "%d", S_COULE);
		}

		printf("Résultat :\n");
		printf("----------\n");
		afficherGrille((*p).grille);
		//		printf(">> %s\n", server_response);
		printf(ANSI_COLOR_BLUE "\nIl vous reste %i points...\n" ANSI_COLOR_RESET, calcAlive(*p));
		if (calcAlive(*p) == 0) {
			printf(ANSI_COLOR_RED"...Vous n'avez plus de bateaux. Vous avez perdu.\n"ANSI_COLOR_RESET);
			sprintf(server_response, "%d", S_PERDU);
		}
		// Envoi d'une réponse au client
		write(is.client_sock, server_response, sizeof (char)*4);

		if (calcAlive(*p) == 0) {
			return S_PERDU;
		}

		break;
	}

	if (is.read_size == 0) {
		puts("Déconnexion du client");
		fflush(stdout);
	} else if (is.read_size == -1) {
		perror("Erreur lors de la réception");
	}

	return status;
}


 //Initialise le client

int initClient(int port) {
	int sock;
	struct sockaddr_in server;

	//Creation du socket
	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock == -1) {
		printf("erreur à la création du socket");
	}
	puts("Socket en service");

	server.sin_addr.s_addr = inet_addr(CLIENT_ADDR); //définition de l'adresse IP du serveur
	server.sin_family = AF_INET;
	server.sin_port = htons(port); //définition du port d'écoute du serveur

	puts("En attente de l'autre joueur...");
	while (connect(sock, (struct sockaddr *) &server, sizeof (server)) < 0) {
	}

	puts("Connexion etablie\n");

	return 0;
}


 //Ouverture d'une connexion au serveur

int connexionServer(int server_port, int wait) {
	struct sockaddr_in server;
	int sock;

	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock == -1) {
		printf("erreur à la création du socket");
	}

	// Connexion au serveur
	server.sin_addr.s_addr = inet_addr(CLIENT_ADDR); //définition de l'adresse IP du serveur
	server.sin_family = AF_INET;
	server.sin_port = htons(server_port); //définition du port d'écoute du serveur

	if (wait == 0) {
		if (connect(sock, (struct sockaddr *) &server, sizeof (server)) < 0) {
			puts("Le serveur n'est pas accessible.");
			return -1;
		}
	} else {
		while (connect(sock, (struct sockaddr *) &server, sizeof (server)) < 0) {

		}
	}
	return sock;
}

// Envoie un message au serveur et affiche la réponse de ce dernier.
int strike(int other_port, Plateau * p) {
	char message[3 + 1], server_reply[3 + 1];
	int sock, srvR;
	Coordonnees c;

	//Creation du socket
	sock = connexionServer(other_port, 0);

	/*
	 * "Interface graphique"
	 */
	printf("\nGrille de l'adversaire :\n");
	printf("------------------------\n");
	afficherGrille((*p).grilleEnnemie);
	printf("\nA vous de jouer !\n");
	printf("Entrez les coordonnées à frapper (ex: a1) : ");
	scanf("%s", message);
	c = strToCoord(message, 0);

	//Envoi de la chaine saisie par l'utilisateur
	if (send(sock, message, strlen(message), 0) < 0) {
		puts("erreur lors de l'envoi");
		return -1;
	}

	//Réception du message de retour du serveur
	if (recv(sock, server_reply, sizeof (char)*4, 0) < 0) {
		puts("erreur lors de la reception de la reponse serveur");
		return -1;
	} else {
		// Conversion réponse
		srvR = strtol(server_reply, NULL, 10);
		printf("Résultat :\n");
		printf("----------\n");
		//		printf("[%i]", srvR);
		switch (srvR) {
			case S_MANQUE:
				printf(ANSI_COLOR_BLUE " > Dans l'eau...\n" ANSI_COLOR_RESET);
				(*p).grilleEnnemie[c.y][c.x] = C_EAU_T;
				break;
			case S_TOUCHE:
				printf(ANSI_COLOR_GREEN " > Touché...\n"ANSI_COLOR_RESET);
				(*p).grilleEnnemie[c.y][c.x] = C_BAT_T;
				break;
			case S_COULE:
				printf(ANSI_COLOR_GREEN " > Coulé !\n"ANSI_COLOR_RESET);
				(*p).grilleEnnemie[c.y][c.x] = C_BAT_T;
				break;
			case S_TOUCHE_BIS:
				printf(ANSI_COLOR_BLUE " ! Vous avez déjà touché ici...\n" ANSI_COLOR_RESET);
				break;
			case S_PERDU:
				printf(ANSI_COLOR_GREEN);
				printf("=============================\n\n");
				printf("  Vous avez gagné ! Bravo !\n");
				printf("=============================\n\n");
				printf(ANSI_COLOR_RESET);
				break;
			default:
				printf(ANSI_COLOR_RED " ! Réponse incompréhensible. (%i)\n" ANSI_COLOR_RESET, srvR);
		}

		return srvR;
	}

}


  //Tente de se conncter au serveur

void handshake(int port) {
	int sock, msgSize = (sizeof (char)*3);
	char message[2 + 1], server_reply[2 + 1];
	int srvV;

	sprintf(message, "%d", S_HANDSHAKE);

	printf("En attente de l'autre joueur...\n");

	sock = connexionServer(port, 1);
	//Envoi du code de poignée de main

	if (send(sock, message, msgSize, 0) < 0) {
		puts("Erreur lors de l'envoi de la poignée de main");
	}

	//Réception du message de retour du serveur
	if (recv(sock, server_reply, msgSize, 0) < 0) {
		puts("Erreur lors de la reception de la reponse serveur");
	} else {
		srvV = strtol(server_reply, NULL, 10);
		if (srvV != S_HANDSHAKE) {
			printf("Réponse du serveur inconnue (%i)\n", srvV);
		} else {
			printf("Connecté à l'autre joueur\n");
		}
	}
	close(sock);
}


 // Attend la connection du client

int wait_handshake(InfosServer is) {
	int content;
	char resp[2 + 1];
	sprintf(resp, "%d", S_HANDSHAKE);

	printf("En attente de l'autre joueur...\n");
	// Acceptation de la connexion d'un client
	is.client_sock = accept(is.socket_desc, (struct sockaddr *) &is.client, (socklen_t*) & is.c);
	if (is.client_sock < 0) {
		perror("Connexion entrante refusee");
		return 1;
	}

	while ((is.read_size = recv(is.client_sock, is.client_message, 100, 0)) > 0) {
		// Traitement du retours
		content = strtol(is.client_message, NULL, 10);
		if (content == S_HANDSHAKE) {
			write(is.client_sock, resp, sizeof (int));
			printf("Connecté à l'autre joueur...\n");
		}
	}
}


 // la fonction main
 // Appel toutes les fonctions dans la fonction main

int main() {
	int t, self_port, other_port, status, perdu;



	 // Saisie du type d'instance.

	printf("Joueur 1 ou 2 ? [1/2] > ");
	scanf("%i", &t);

	if (t == 1) {
		self_port = SRV_PORT;
		other_port = CLT_PORT;
	} else {
		self_port = CLT_PORT;
		other_port = SRV_PORT;
	}


	 // Initialisation du serveur

	InfosServer srv = initServer(self_port);


	 // Création de la partie

	// Génération de la grille
	Plateau plateau = initPlateau();

	printf("Placement de vos bateaux\n");
	printf("------------------------\n");

	// Placement des bateaux

	plateau = placeShip(plateau, "porte avions", B_PORTE_AVION, C_PORTE_AVION);
	plateau = placeShip(plateau, "sous-marin", B_SOUS_MARIN, C_SOUS_MARIN);
	plateau = placeShip(plateau, "torpilleur", B_TORPILLEUR, C_TORPILLEUR);



	printf("\n==============================================================\n");
	printf(" Et c'est parti !");
	printf("\n==============================================================\n");

	status = S_TOUCHE;
	// Décalage entre joueur 1 et 2
	if (t == 1) {
		// Mode "écoute"
		wait_handshake(srv);
		afficherGrille(plateau.grille);
		while (status != S_PERDU) {
			// Tant que l'autre touche
			do {
				status = receptionMessageClient(srv, &plateau);
			} while (status == S_TOUCHE || status == S_COULE);
			if (status != S_PERDU) {
				do {
					status = strike(other_port, &plateau);
				} while (status == S_TOUCHE || status == S_COULE);
			}
		}
	} else {
		// Mode "envoi"
		handshake(other_port);
		while (status != S_PERDU) {
			do {
				status = strike(other_port, &plateau);
			} while (status == S_TOUCHE || status == S_COULE);

			if (status != S_PERDU) {
				do {
					status = receptionMessageClient(srv, &plateau);
				} while (status == S_TOUCHE || status == S_COULE);
			}
		}
	}

	closeApp(srv, 0, "Bye.");

	return (EXIT_SUCCESS);
}
