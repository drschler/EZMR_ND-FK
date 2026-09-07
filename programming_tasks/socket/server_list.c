#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <stdbool.h>

#include <unistd.h>

#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#define BUFFER_SIZE 512
#define MAX_MESSAGES 100
#define MAX_MESSAGE_LENGTH 256

char list[MAX_MESSAGES][MAX_MESSAGE_LENGTH];
int message_count = 0;

//globale Var
int server_socket = -1;
int client_socket = -1;

void signal_handler()
{
	printf("\nServer wird beendet...\n");

	if (client_socket != -1)
	{
		close(client_socket);
	}

	if (server_socket != -1)
	{
		close(server_socket);
	}

	exit(0);
}

void send_end()
{
	send(client_socket, "END\n", 4, 0);
}

void send_text(const char *text, bool end)
{
	char buffer[BUFFER_SIZE];

	snprintf(buffer, sizeof(buffer), "%s\n", text);

	send(client_socket, buffer, strlen(buffer), 0);
	
	if (end)
	{
		send_end();
	}
}

int add_message(const char *text)
{
	if(text == NULL || strlen(text) == 0)
		return -1;

	if(message_count >= MAX_MESSAGES)
		return -2;

	if(strlen(text) >= MAX_MESSAGE_LENGTH)
		return -3;

	strncpy(list[message_count], text, MAX_MESSAGE_LENGTH - 1);
	list[message_count][MAX_MESSAGE_LENGTH - 1] = '\0';

	message_count++;

	return 0;
}

void send_list()
{
	if(message_count == 0)
	{
		send_text("Liste ist leer", true);
	}
	else
	{
		for(int i = 0; i < message_count; i++)
		{
			char buffer[BUFFER_SIZE];

			snprintf(buffer,
			         sizeof(buffer),
			         "Eintrag Nr. %d: %.*s",
			         i + 1,
			         MAX_MESSAGE_LENGTH - 1,
			         list[i]);

			send_text(buffer, false);
		}
		send_end();
	}
}

int main(void)
{
	signal(SIGINT, signal_handler);

	struct sockaddr_in server_addr;
	memset(&server_addr, 0, sizeof(server_addr)); //komplette Struktur resetten

	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(8080);
	//server_addr.sin_addr.s_addr = INADDR_ANY;
	inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);

	char buffer[BUFFER_SIZE];
	int bytes;

	server_socket = socket(AF_INET, SOCK_STREAM, 0);

	//fehlerhafte Definition des Sockets abchecken
	if(server_socket < 0)
	{
		perror("Socket konnte nicht erstellt werden"); 	
		return 1;
	}
	printf("Socket erstellt!\n");



	if (bind(server_socket,
		(struct sockaddr *)&server_addr,
		sizeof(server_addr)) < 0)
	{
		perror("Bind fehlgeschlagen");
		return 1;
	}
	printf("Bind erfolgreich!\n");



	if (listen(server_socket, 3) < 0)
	{
		perror("Listen fehlgeschlagen");
		return 1;
	}
	printf("Server wartet auf Verbindungen...\n");



	while (1)
	{
		struct sockaddr_in client_addr;
		socklen_t client_addr_len = sizeof(client_addr);

		printf("Warte auf Client...\n");

		client_socket = accept(server_socket,
							(struct sockaddr *)&client_addr,
							&client_addr_len);

		if (client_socket < 0)
		{
			perror("Accept fehlgeschlagen");
			continue;
		}

		printf("Client verbunden!\n");
		
		send_text("\nHallo, du hast dich erfolgreich mit mir verbunden, dem Server.", false); 
		send_text("Deshalb bekommst du kurz einen Überblick über die möglich Eingaben", false);
		send_text("'PING' -> Test", false);
		send_text("'ADD <Text>' -> Eintrag in eine Liste hinzufügen", false);
		send_text("'LIST' -> Alle Einträge der Liste ausgeben\n", true);


		//Datentransfer und -verarbeitung
		while (1) 
		{
	
			bytes = recv(client_socket,
					buffer,
					sizeof(buffer)-1,
					0);

			if (bytes == 0)
			{
				printf("Client hat Verbindung beendet.\n");
				break;
			}

			if (bytes < 0)
			{
				perror("Recv fehlgeschlagen");
				break;
			}

			// String terminieren
			buffer[bytes] = '\0';

			// Zeilenumbruch entfernen
			buffer[strcspn(buffer, "\r\n")] = '\0';
		
			if (strcmp(buffer, "PING") == 0)
			{
				send_text("PONG", true);
			}
			else if (strncmp(buffer, "ADD ", 4) == 0)
			{
				char *text = buffer + 4;

				int result = add_message(text);

				switch(result)
				{
				case 0:
					send_text("Eintrag in Liste", true);
					break;
				case -1:
					send_text("Nachricht besitzt keinen Inhalt", true);
					break;

				case -2:
					send_text("Liste voll", true);
					break;	
	
				case -3:
					send_text("Nachricht zu lang", true);
					break;
				}
			}
			else if (strcmp(buffer, "LIST") == 0)
			{
				send_list();
			}
			else
			{
			    send_text("Sinnlose Eingabe, Junge", true);
			}

		}
	}
}
