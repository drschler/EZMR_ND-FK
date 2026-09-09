#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <string.h>

#include "header.h"

int main()
{
    key_t key;
    int shmid;
    pdu_t *shared_memory;

    // Gleichen Schlüssel erzeugen wie der Server
    key = ftok("server.c", 65);

    if (key == -1)
    {
        perror("ftok");
        return 1;
    }

    // Vorhandenes Shared Memory holen
    shmid = shmget(key, sizeof(pdu_t), 0666);

    if (shmid == -1)
    {
        perror("shmget");
        return 1;
    }

    // Shared Memory anbinden
    shared_memory = (pdu_t *) shmat(shmid, NULL, 0);

    if (shared_memory == (void *) -1)
    {
        perror("shmat");
        return 1;
    }

	//==========Eingabe============
	int choice;

	printf("\nWas möchtest du tun?\n");
	printf("1 - PING\n");
	printf("2 - TEXT\n");
	printf("3 - EXIT\n");
	printf("Auswahl: ");

	if (scanf("%d", &choice) != 1)
	{
		printf("Ungültige Eingabe.\n");
		shmdt(shared_memory);
		return 1;
	}

	shared_memory->type = choice;

	//für Data
	switch (choice)
	{
		case 1:
			// PING braucht keine zusätzlichen Daten
			shared_memory->data[0] = '\0';
			break;

		case 2:
			printf("Text eingeben: ");

			// Restliches '\n' von scanf entfernen - kommt durchs Enter drücken
			getchar();

			if (fgets(shared_memory->data,
					sizeof(shared_memory->data),
					stdin) == NULL)
			{
				printf("Fehler beim Einlesen des Textes.\n");
				shmdt(shared_memory);
				return 1;
			}
			// \n von fgets entfernen
			shared_memory->data[strcspn(shared_memory->data, "\n")] = '\0';

			break;

		case 3:
			// EXIT braucht keine zusätzlichen Daten
			shared_memory->data[0] = '\0';
			break;

		default:
			printf("Ungültige Auswahl.\n");
			shmdt(shared_memory);
			return 1;
	}

	// Server informieren -> Startschuss
	shared_memory->status = 1;

	// Auf Antwort warten
	while (shared_memory->status != 2)
	{
	}

	// Antwort lesen
	printf("Antwort vom Server: %s\n", shared_memory->data);

	// Kommunikation abgeschlossen
	shared_memory->status = 0;

    // Shared Memory wieder lösen
    shmdt(shared_memory);

    return 0;
}