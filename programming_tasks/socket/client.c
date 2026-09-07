#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#include <unistd.h>

#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

//globale Var
int client_socket = -1;

//###################### Prototypen ######################
void signal_handler();
void recieve(int socket);
ssize_t recv_line(int socket_fd, char *buffer, size_t buffer_size);


//###################### Main ######################
int main(void)
{
	signal(SIGINT, signal_handler);

	struct sockaddr_in server_addr;
	memset(&server_addr, 0, sizeof(server_addr));

	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(8080);
	//server_addr.sin_addr.s_addr = INADDR_ANY;
	inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);

	client_socket = socket(AF_INET, SOCK_STREAM, 0);

	if(client_socket < 0)
	{
		perror("Socket konnte nicht erstellt werden"); 	
		return 1;
	}
	printf("Socket erstellt!\n");


	if (connect(client_socket,
		(struct sockaddr *)&server_addr,
		sizeof(server_addr)) < 0)
	{
	    perror("Verbindungsaufbau fehlgeschlagen");
	    return 1;
	}

	recieve(client_socket);

	printf("\nEingabe: ");


	char input[512];
	
	while (fgets(input, sizeof(input), stdin) != NULL)
	{
		send(client_socket, input, strlen(input), 0);
		
		recieve(client_socket);
		
		printf("\nEingabe: ");
	}
}


//###################### Funktionen ######################
void signal_handler()
{
    printf("\nClient wird beendet...\n");

    if (client_socket != -1)
    {
        close(client_socket);
    }

    exit(0);
}

void recieve(int socket)
{
	//empfangene solange Byte-Strom vom Server, bis "END" empfangen wird
	printf("Nachricht vom Server:\n");
	while(1)
	{
		char buffer[1024];

		ssize_t bytes = recv_line(socket,
									buffer,
									sizeof(buffer));

		if (bytes < 0)
		{
			perror("\nEmpfangen fehlgeschlagen");
			break;
		}

		if (bytes == 0)
		{
			printf("\nServer hat die Verbindung geschlossen.\n");
			break;
		}


		buffer[strcspn(buffer, "\r\n")] = '\0';

		if (strcmp(buffer, "END") == 0)
		{
		break;
		}

		printf("%s\n", buffer);
	}
}

ssize_t recv_line(int socket_fd, char *buffer, size_t buffer_size)
{
	//empfangenen Byte-Strom wieder in Nachrichten zerlegen
	size_t pos = 0;

	if (buffer == NULL || buffer_size == 0)
	{
		return -1;
	}

	while (pos < buffer_size - 1)
	{
		char character;

		ssize_t bytes = recv(socket_fd, &character, 1, 0);

		if (bytes < 0)
		{
			return -1;
		}

		if (bytes == 0)
		{
			break;
		}

		buffer[pos++] = character;

		if (character == '\n')
		{
			break;
		}
	}

	buffer[pos] = '\0';

	return (ssize_t)pos;
}

