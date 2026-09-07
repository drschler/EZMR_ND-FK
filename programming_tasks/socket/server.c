#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#include <unistd.h>

#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

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

int main(void)
{
	signal(SIGINT, signal_handler);

	struct sockaddr_in server_addr;
	memset(&server_addr, 0, sizeof(server_addr)); //komplette Struktur resetten

	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(8080);
	//server_addr.sin_addr.s_addr = INADDR_ANY;
	inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);

	char buffer[1024];
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

			buffer[bytes] = '\0';

			//printf("Empfangen: %s\n", buffer);
			
			if (strncmp(buffer, "PING", 4) == 0)
			{
				send(client_socket, "PONG\n", 5, 0);
			}
			else if (strncmp(buffer, "TEST", 4) == 0)
			{
				send(client_socket, "HALLI HALLO\n", 12, 0);
			}
			else
			{
				send(client_socket, "sinnlose EINGABE junge\n", 23, 0);
			}
	    }
	    close(client_socket);
	}
}
