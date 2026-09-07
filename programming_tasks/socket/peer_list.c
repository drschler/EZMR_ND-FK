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
#define MAX_ENTRIES 11
#define MAX_ID_LENGTH 64
#define MAX_TEXT_LENGTH 256


int listen_socket = -1;
int own_port = -1;
int entry_count = 0;
int local_entry_counter = 0;

struct peer
{
    int socket;
    char ip[INET_ADDRSTRLEN];
    int port;
};

struct peer peers[MAX_PEERS];

struct list_entry
{
    char id[MAX_ID_LENGTH];
    char text[MAX_TEXT_LENGTH];
};

struct list_entry entries[MAX_ENTRIES];

	
//###################### Prototyp ######################
void signal_handler();
void broadcast_entry(const char *id, const char *text, int except_peer);
void recieve_message(const char *message, int peer_index);
int create_listen_socket(int port);
int connect_to_peer(const char *ip, int port);
int add_entry(const char *id, const char *text);
int connect_send_hello(int socket);


//###################### Main ######################
int main(int arg_counter, char *arg_vektor[])
{
	signal(SIGINT, signal_handler);

	for (int i = 0; i < MAX_PEERS; i++)
	{
		peers[i].socket = -1;
		peers[i].ip[0] = '\0';
		peers[i].port = -1;
	}

	if (arg_counter != 2) 
	{
		fprintf(stderr,
			"UPS, da hast du das Programm falsch aufgerufen.\nrichtige Verwendung: %s <eigener Port>\n",
			arg_vektor[0]);
		return 1;
	}

	own_port = atoi(arg_vektor[1]);

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
			if (peers[i].socket >= 0)
			{
				FD_SET(peers[i].socket, &read_fds);

				if (peers[i].socket > max_fd)
				{
					max_fd = peers[i].socket;
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
					if (peers[i].socket == -1)
					{
						peers[i].socket = accept_socket;
						inet_ntop(
							AF_INET,
							&client_address.sin_addr,
							peers[i].ip,
							sizeof(peers[i].ip)
						);
						peers[i].port = -1; //vorerst -1, wird später durch HELLO-Nachricht gesetzt
						
						stored = 1;

						printf("Peer %d verbunden.\n", i);

						connect_send_hello(accept_socket);

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
							if (peers[i].socket == -1)
							{
								peers[i].socket = connect_socket;
								snprintf(peers[i].ip, sizeof(peers[i].ip), "%s", ip);
								peers[i].port = port;

								stored = 1;

								printf("Verbindung als Peer %d gespeichert.\n", i);

								connect_send_hello(connect_socket);

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
					else if (peers[peer_index].socket < 0)
					{
						printf("Peer %d ist nicht verbunden.\n", peer_index);
					}
					else
					{
						ssize_t sent_bytes = send(
							peers[peer_index].socket,
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
					if (peers[i].socket >= 0)
					{
						if (send(
								peers[i].socket,
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
			else if (strncmp(input, "add ", 4) == 0)
			{
				char *text = input + 4;

				if (strlen(text) == 0)
				{
					printf("korrekte Verwendung: add <Text>\n");
				}
				else
				{
					char id[MAX_ID_LENGTH];

					local_entry_counter++;

					snprintf(
						id,
						sizeof(id),
						"peer-%d-%d",
						own_port,
						local_entry_counter
					);

					int add_result = add_entry(id, text);

					if (add_result == 1)
					{
						printf(
							"Eintrag hinzugefügt: [%s] %s\n",
							id,
							text
						);

						broadcast_entry(id, text, -1); //senden an alle Peers, für except_peer -1
					}
				}
			}
			else if (strcmp(input, "list") == 0)
			{
				if (entry_count == 0)
				{
					printf("Die Liste ist leer.\n");
				}
				else
				{
					for (int i = 0; i < entry_count; i++)
					{
						printf(
							"%d: [%s] %s\n",
							i,
							entries[i].id,
							entries[i].text
						);
					}
				}
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
					else if (peers[peer_index].socket < 0)
					{
						printf("Peer %d ist nicht verbunden.\n", peer_index);
					}
					else
					{
						close(peers[peer_index].socket);
						peers[peer_index].socket = -1;
						peers[peer_index].ip[0] = '\0';
						peers[peer_index].port = -1;

						printf("Verbindung zu Peer %d getrennt.\n", peer_index);
					}
				}
				else
				{
					printf("Verwendung: disconnect <Peer-Nr>\n");
				}
			}
			else if (strcmp(input, "peer-list") == 0)
			{
				int count = 0;

				for (int i = 0; i < MAX_PEERS; i++)
				{
					if (peers[i].socket >= 0)
					{
						printf(
							"Peer %d: Socket %d | IP: %s | Port: %d\n",
							i,
							peers[i].socket,
							peers[i].ip,
							peers[i].port
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
				printf("  add <Text>\n");
				printf("  list\n");
				printf("  disconnect <Peer-Nr>\n");
				printf("  peer-list\n");
			}
		}

		//Eingang von Daten prüfen
		for (int i = 0; i < MAX_PEERS; i++)
		{
			int recv_socket = peers[i].socket;

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

					close(peers[i].socket);

					peers[i].socket = -1;
					peers[i].ip[0] = '\0';
					peers[i].port = -1;
				}
				else
				{
					receive_buffer[received_bytes] = '\0';

					recieve_message(receive_buffer, i);
				}
			}
		}
	}

	close(listen_socket);
	return 0;
}







//###################### Funktionen ######################
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
	address.sin_addr.s_addr = INADDR_ANY; //jede Adresse akzeptieren
	//inet_pton(AF_INET, "127.0.0.1", &address.sin_addr);
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

void signal_handler()
{
    printf("\nPeer-Anwendung wird beendet...\n");

	if (listen_socket != -1)
	{
		close(listen_socket);
	}

	for (int i = 0; i < MAX_PEERS; i++)
	{
		if (peers[i].socket != -1)
		{
			close(peers[i].socket);
		}
	}


	exit(0);
}

int entry_exists(const char *id)
{
	for (int i = 0; i < entry_count; i++)
	{
		if (strcmp(entries[i].id, id) == 0)
		{
			return 1; //Eintrag existiert
		}
	}
	return 0; //Eintrag existiert nicht
}

int add_entry(const char *id, const char *text)
{
	if (entry_exists(id))
    {
		printf("Fehler: Eintrag mit ID '%s' existiert bereits.\n", id);
        return 0; //Eintrag existiert bereits
    }

	if (entry_count >= MAX_ENTRIES)
	{
		printf("Fehler: Liste ist voll.\n");
		return -1; //Liste voll
	}

	if (strlen(id) == 0)
	{
		printf("Fehler: ID ist leer.\n");
		return -2; //ID leer
	}

	if (strlen(id) >= MAX_ID_LENGTH || strlen(text) >= MAX_TEXT_LENGTH)
	{
		printf("Fehler: ID oder Text ist zu lang.\n");
		return -3; //ID oder Text zu lang
	}

	snprintf(
        entries[entry_count].id,
        sizeof(entries[entry_count].id),
        "%s",
        id
    );

    snprintf(
        entries[entry_count].text,
        sizeof(entries[entry_count].text),
        "%s",
        text
    );

	entry_count++;

	return 1; //Erfolg
}

void broadcast_entry(const char *id, const char *text, int except_peer)
{
    char message[BUFFER_SIZE];

    int length = snprintf(
        message,
        sizeof(message),
        "ADD|%s|%s\n",
        id,
        text
    );

    if (length < 0 || length >= (int)sizeof(message))
    {
        printf("Synchronisationsnachricht ist zu lang.\n");
        return;
    }

    for (int i = 0; i < MAX_PEERS; i++)
    {
        if (peers[i].socket >= 0 && i != except_peer)
        {
            if (send(
                    peers[i].socket,
                    message,
                    strlen(message),
                    0
                ) < 0)
            {
                perror("Eintrag konnte nicht synchronisiert werden");
            }
        }
    }
}

void recieve_message(const char *message, int sender_peer)
{
    if (strncmp(message, "HELLO|", 6) == 0)
    {
        int peer_port;

        if (sscanf(message, "HELLO|%d", &peer_port) == 1)
        {
            peers[sender_peer].port = peer_port;

            printf(
                "Peer %d vorgestellt: %s:%d\n",
                sender_peer,
                peers[sender_peer].ip,
                peers[sender_peer].port
            );
        }
        else
        {
            printf(
                "Ungültige HELLO-Nachricht von Peer %d.\n",
                sender_peer
            );
        }
    }
    else if (strncmp(message, "ADD|", 4) == 0)
    {
        char message_copy[BUFFER_SIZE];

        snprintf(
            message_copy,
            sizeof(message_copy),
            "%s",
            message
        );

        message_copy[strcspn(message_copy, "\r\n")] = '\0';

        char *id = strtok(message_copy + 4, "|");
        char *text = strtok(NULL, "");

        if (id == NULL || text == NULL)
        {
            printf("Ungültige ADD-Nachricht erhalten.\n");
            return;
        }

        int result = add_entry(id, text);

        if (result == 1)
        {
            printf(
                "Synchronisierter Eintrag von Peer %d: [%s] %s\n",
                sender_peer,
                id,
                text
            );

			//Synchronisieren mit restlichen Peers
            broadcast_entry(id, text, sender_peer);
        }
        else if (result == 0)
        {
            printf(
                "Eintrag [%s] war bereits bekannt und wurde ignoriert.\n",
                id
            );
        }
    }
    else
    {
        printf(
            "Nachricht von Peer %d: %s\n",
            sender_peer,
            message
        );
    }
}

int connect_send_hello(int socket)
{
    char message[64];

    int length = snprintf(
        message,
        sizeof(message),
        "HELLO|%d\n",
        own_port
    );

    if (length < 0 || length >= (int)sizeof(message))
    {
        printf("HELLO-Nachricht konnte nicht erstellt werden.\n");
        return -1;
    }

    ssize_t sent_bytes = send(
        socket,
        message,
        strlen(message),
        0
    );

    if (sent_bytes < 0)
    {
        perror("HELLO konnte nicht gesendet werden");
        return -1;
    }

    return 0;
}