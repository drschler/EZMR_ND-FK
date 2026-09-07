#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <signal.h>

#define BUFFER_SIZE 512
#define MAX_PEERS 3

int create_listen_socket(int port);
int connect_to_peer(const char *ip, int port);

int listen_socket = -1;
int peer_sockets[MAX_PEERS];

void signal_handler()
{
    printf("\nPeer-Anwendung wird beendet...\n");

	if (listen_socket != -1)
	{
		close(listen_socket);
	}

	for (int i = 0; i < MAX_PEERS; i++)
	{
		if (peer_sockets[i] != -1)
		{
			close(peer_sockets[i]);
		}
	}


	exit(0);
}


int main(int arg_counter, char *arg_vektor[])
{
	signal(SIGINT, signal_handler);

	for (int i = 0; i < MAX_PEERS; i++)
	{
		peer_sockets[i] = -1;
	}	

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

	// read_fds deklariert als Typ fd_set -> Menge an Dateideskriptoren
	fd_set read_fds;

	while (1)
	{
		FD_ZERO(&read_fds);

		FD_SET(STDIN_FILENO, &read_fds); //beobachte Tastatureingabe
		FD_SET(listen_socket, &read_fds); //beobachtet den Listening-Socket
	
		int max_fd = listen_socket;

		for (int i = 0; i < MAX_PEERS; i++)
		{
			if (peer_sockets[i] >= 0)
			{
				FD_SET(peer_sockets[i], &read_fds);

				if (peer_sockets[i] > max_fd)
				{
					max_fd = peer_sockets[i];
				}
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
	
			//Socket für Kommunikation
			int accept_socket = accept(
				listen_socket,
				(struct sockaddr *)&client_address,
				&client_length
			);

			if (accept_socket < 0)
			{
				perror("accept fehlgeschlagen");
			}
			else
			{
				int stored = 0;

				for (int i = 0; i < MAX_PEERS; i++)
				{
					if (peer_sockets[i] == -1)
					{
						peer_sockets[i] = accept_socket;
						stored = 1;

						printf("Peer %d verbunden.\n", i);
						break;
					}
				}

				if (!stored)
				{
					printf("Maximale Anzahl an Peers erreicht.\n");
					close(accept_socket);
				}
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

				if (sscanf(input, "connect %15s %d", ip, &port) == 2)
				{
					int connect_socket = connect_to_peer(ip, port);

					if (connect_socket >= 0)
					{
						int stored = 0;

						for (int i = 0; i < MAX_PEERS; i++)
						{
							if (peer_sockets[i] == -1)
							{
								peer_sockets[i] = connect_socket;
								stored = 1;

								printf("Verbindung als Peer %d gespeichert.\n", i);
								break;
							}
						}

						if (!stored)
						{
							printf("Keine freien Peer-Plätze mehr.\n");
							close(connect_socket);
						}
					}
				}
				else
				{
					printf("korrekte Verwendung: connect <IP> <Port>\n");
				}
			}
			else if (strncmp(input, "send ", 5) == 0)
			{
				int peer_index;
				char message[BUFFER_SIZE];

				if (sscanf(
						input,
						"send %d %511[^\n]",
						&peer_index,
						message
					) == 2)
				{
					if (peer_index < 0 || peer_index >= MAX_PEERS)
					{
						printf("Ungültige Peer-Nr.\n");
					}
					else if (peer_sockets[peer_index] < 0)
					{
						printf("Peer %d ist nicht verbunden.\n", peer_index);
					}
					else
					{
						ssize_t sent_bytes = send(
							peer_sockets[peer_index],
							message,
							strlen(message),
							0
						);

						if (sent_bytes < 0)
						{
							perror("send fehlgeschlagen");
						}
						else
						{
							printf("Nachricht an Peer %d gesendet.\n", peer_index);
						}
					}
				}
				else
				{
					printf("korrekte Verwendung: send <Peer-Nr> <Nachricht>\n");
				}
			}
			else if (strncmp(input, "broadcast ", 10) == 0)
			{
				char *message = input + 10;
				int send_count = 0;

				for (int i = 0; i < MAX_PEERS; i++)
				{
					if (peer_sockets[i] >= 0)
					{
						if (send(
								peer_sockets[i],
								message,
								strlen(message),
								0
							) < 0)
						{
							perror("broadcast send fehlgeschlagen");
						}
						else
						{
							send_count++;
						}
					}
				}

				printf("Nachricht an %d Peer(s) gesendet.\n", send_count);
			}
			else if (strncmp(input, "disconnect ", 11) == 0)
			{
				int peer_index;

				if (sscanf(input, "disconnect %d", &peer_index) == 1)
				{
					if (peer_index < 0 || peer_index >= MAX_PEERS)
					{
						printf("Ungültige Peer-Nr.\n");
					}
					else if (peer_sockets[peer_index] < 0)
					{
						printf("Peer %d ist nicht verbunden.\n", peer_index);
					}
					else
					{
						close(peer_sockets[peer_index]);
						peer_sockets[peer_index] = -1;

						printf("Verbindung zu Peer %d getrennt.\n", peer_index);
					}
				}
				else
				{
					printf("Verwendung: disconnect <Peer-Nr>\n");
				}
			}
			else if (strcmp(input, "list") == 0)
			{
				int count = 0;

				for (int i = 0; i < MAX_PEERS; i++)
				{
					if (peer_sockets[i] >= 0)
					{
						printf(
							"Peer %d: Socket(Dateideskriptor im Select) %d\n",
							i,
							peer_sockets[i]
						);

						count++;
					}
				}

				if (count == 0)
				{
					printf("Keine Peers verbunden.\n");
				}
			}
			else
			{
				printf("ungültige Eingabe!\n\n");	
				printf("Befehle:\n");
				printf("  connect <IP> <Port>\n");
				printf("  send <Peer-Nr> <Nachricht>\n");
				printf("  broadcast <Nachricht>\n");
				printf("  disconnect <Peer-Nr>\n");
				printf("  list\n");
			}
		}

		//Eingang von Daten prüfen
		for (int i = 0; i < MAX_PEERS; i++)
		{
			int recv_socket = peer_sockets[i];

			if (recv_socket >= 0 && FD_ISSET(recv_socket, &read_fds))
			{
				char receive_buffer[BUFFER_SIZE];

				ssize_t received_bytes = recv(
					recv_socket,
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
					printf("Peer %d hat die Verbindung geschlossen.\n", i);

					close(recv_socket);
					peer_sockets[i] = -1;
				}
				else
				{
					receive_buffer[received_bytes] = '\0';

					printf(
						"Nachricht von Peer %d: %s\n",
						i,
						receive_buffer
					);
				}
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