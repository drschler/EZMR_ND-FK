#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <signal.h>

#define BUFFER_SIZE 512

int create_listen_socket(int port);
int connect_to_peer(const char *ip, int port);

int listen_socket = -1;
int peer_socket = -1;

void signal_handler()
{
    printf("\nPeer-Anwendung wird beendet...\n");

	if (listen_socket != -1)
	{
		close(listen_socket);
	}

	if (peer_socket != -1)
	{
		close(peer_socket);
	}

	exit(0);
}

int main(int arg_counter, char *arg_vektor[])
{
	signal(SIGINT, signal_handler);

	if (arg_counter != 2) 
	{
		fprintf(stderr,
			"UPS, da hast du das Programm falsch aufgerufen.\nrichtige Verwendung: %s <eigener Port>\n",
			arg_vektor[0]);
		return 1;
	}

	int own_port = atoi(arg_vektor[1]);

	listen_socket = create_listen_socket(own_port);

	if (listen_socket < 0) 
	{
		return 1;
	}

	printf("Peer wartet auf Port %d auf Verbindungen.\n", own_port);

	fd_set read_fds;
	//read_fds -> Dateideskriptor

	while (1)
	{
		FD_ZERO(&read_fds);

		FD_SET(STDIN_FILENO, &read_fds); //beobachte Tastatureingabe
		FD_SET(listen_socket, &read_fds); //beobachte den Listening-Socket
	
		int max_fd = listen_socket;

		if (peer_socket >= 0) 
		{
			FD_SET(peer_socket, &read_fds);

			if (peer_socket > max_fd) 
			{
				max_fd = peer_socket;
			}
		}

		//select() aufrufen -> führt die Operationen nicht selbst aus, gibt an, welche Op. ohne blocking ausgeführt werden sollen
		int result = select(
			max_fd + 1, //Anzahl der zu überprüfenden Dateideskriptoren
			&read_fds, //Dateideskriptoren, bei denen aus Lesbarkeit gewartet wird
			NULL, //Dateideskriptoren, bei denen aus Beschreibbarkeit gewartet wird
			NULL, //außergewöhnliche Zustände bzw. Fehler
			NULL //Wartezeit -> hier unbegrenzt
		);

		if (result < 0) {
			perror("select fehlgeschlagen");
			break;
		}
		
		//Verbindungsanfrage prüfen
		if (FD_ISSET(listen_socket, &read_fds)) 
		{
			struct sockaddr_in client_address;
			socklen_t client_length = sizeof(client_address);
	
			//peer_socket für Kommunikation
			peer_socket = accept(
				listen_socket,
				(struct sockaddr *)&client_address,
				&client_length
			);

			if (peer_socket < 0) 
			{
				perror("accept fehlgeschlagen");
			}
			else 
			{
				printf("Neue Verbindung angenommen!\n");
			}
		}

		//Tastureingabe prüfen
		if (FD_ISSET(STDIN_FILENO, &read_fds))
		{
			char input[BUFFER_SIZE];

			if (fgets(input, sizeof(input), stdin) == NULL)
			{
					break;
			}

			input[strcspn(input, "\n")] = '\0';

			if (strncmp(input, "connect ", 8) == 0)
			{
				char ip[INET_ADDRSTRLEN];
				int port;

				if (peer_socket >= 0)
				{
					printf("Es besteht bereits eine Verbindung.\n");
				}
				else if (sscanf(
							input,
							"connect %15s %d",
							ip,
							&port
							) == 2)
				{
					peer_socket = connect_to_peer(ip, port);
				}
				else
				{
					printf("Verwendung: connect <IP> <Port>\n");
				}
			}
			else if (strncmp(input, "send ", 5) == 0)
			{
				if (peer_socket < 0)
				{
					printf("Es besteht keine Verbindung.\n");
				}
				else
				{
					char *message = input + 5;

					if (send(
							peer_socket,
							message,
							strlen(message),
							0
						) < 0)
					{
							perror("send fehlgeschlagen");
					}
				}
			}
			else if (strcmp(input, "disconnect") == 0)
			{
				if (peer_socket < 0)
				{
					printf("Es besteht garkeine Verbindung zum Disconnecten.\n");
				}
				else
				{
					close(peer_socket);
					peer_socket = -1;
					printf("Verbindung getrennt.\n");
				}
			}
			else
			{
				printf("ungültige Eingabe!\n\n");	
				printf("Befehle:\n");
				printf("  connect <IP> <Port>\n");
				printf("  send <Nachricht>\n");
				printf("  disconnect\n");
			}
		}

		//Eingang von Daten prüfen
		if (peer_socket >= 0 && FD_ISSET(peer_socket, &read_fds))
		{
			char receive_buffer[BUFFER_SIZE];

			ssize_t received_bytes = recv(
				peer_socket,
				receive_buffer,
				sizeof(receive_buffer) - 1,
				0
			);

			if (received_bytes < 0)
			{
				perror("recv fehlgeschlagen");
			}
			else if (received_bytes == 0)
			{
				printf("Der Peer hat die Verbindung geschlossen.\n");

				close(peer_socket);
				peer_socket = -1;
			}
			else
			{
				receive_buffer[received_bytes] = '\0';

				printf("Nachricht vom Peer: %s\n", receive_buffer);
			}
		}
	}

	close(listen_socket);
	return 0;
}

int create_listen_socket(int port)
{
	int listen_socket;
	struct sockaddr_in address;

	listen_socket = socket(AF_INET, SOCK_STREAM, 0);

	if (listen_socket < 0) 
	{
		perror("Socket konnte nicht erstellt werden");
		return -1;
	}

	int option = 1;
	
	/*schaltet für den Socket die Option "SO_REUSEADDR" an: 
	erlaubt die IP-Adresse und den Port so schnell wie möglich wiederzuverwenden
	(kein Blockieren nach abschalten)*/
	if (setsockopt(
			listen_socket,
			SOL_SOCKET,
			SO_REUSEADDR,
			&option,
			sizeof(option)) < 0) 
	{
		perror("setsockopt fehlgeschlagen");
		close(listen_socket);
		return -1;
	}

	memset(&address, 0, sizeof(address));

	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons(port);

	if (bind(
		listen_socket,
		(struct sockaddr *)&address,
		sizeof(address)) < 0) 
	{
		perror("Bind fehlgeschlagen");
		close(listen_socket);
		return -1;
	}

	if (listen(listen_socket, 10) < 0) 
	{
		perror("Listen fehlgeschlagen");
		close(listen_socket);
		return -1;
	}

	return listen_socket;
}

int connect_to_peer(const char *ip, int port)
{
	int peer_socket;
	struct sockaddr_in peer_address;

	peer_socket = socket(AF_INET, SOCK_STREAM, 0);

	if (peer_socket < 0) 
	{
		perror("Socket konnte nicht erstellt werden");
		return -1;
	}

	memset(&peer_address, 0, sizeof(peer_address));

	peer_address.sin_family = AF_INET;
	peer_address.sin_port = htons(port);

	if (inet_pton(AF_INET, ip, &peer_address.sin_addr) <= 0) 
	{
		fprintf(stderr, "Ungültige IP-Adresse: %s\n", ip);
		close(peer_socket);
		return -1;
	}

	if (connect(
		peer_socket,
		(struct sockaddr *)&peer_address,
		sizeof(peer_address)) < 0) 
	{
		perror("Verbindungsaufbau fehlgeschlagen");
		close(peer_socket);
		return -1;
	}

	printf("Mit Peer %s:%d verbunden.\n", ip, port);

	return peer_socket;
}