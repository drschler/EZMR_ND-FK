#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#include <unistd.h>

#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>


// globale Variable
int client_socket = -1;


// ###################### Prototypen ######################
void signal_handler();
void recieve(int socket);


// ###################### Main ######################
int main(void)
{
    signal(SIGINT, signal_handler);

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(8080);
    inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);

    client_socket = socket(AF_INET, SOCK_STREAM, 0);

    if (client_socket < 0)
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
        close(client_socket);
        return 1;
    }

    printf("Mit Server verbunden!\n");


    char input[512];

    printf("\nEingabe: ");

    while (fgets(input, sizeof(input), stdin) != NULL)
    {
        // Eingabe an Server senden
        if (send(client_socket,
                 input,
                 strlen(input),
                 0) < 0)
        {
            perror("Senden fehlgeschlagen");
            break;
        }

        recieve(client_socket);

        printf("\nEingabe: ");
    }
}


// ###################### Funktionen ######################

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
    char buffer[1024];

    // Eine Antwort vom Server empfangen
    ssize_t bytes = recv(socket,
                         buffer,
                         sizeof(buffer) - 1,
                         0);

    if (bytes < 0)
    {
        perror("Empfangen fehlgeschlagen");
        return;
    }

    if (bytes == 0)
    {
        printf("Server hat die Verbindung geschlossen.\n");
        return;
    }

    // recv() fügt selbst kein '\0' an
    buffer[bytes] = '\0';

    printf("Nachricht vom Server: %s", buffer);
}