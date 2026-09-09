#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <string.h>

#include "header_list.h"

int main()
{
	//==========Shared Memory============
    key_t key;
    int shmid;
    shared_data_t *shared_memory;

    // Gleichen Schlüssel erzeugen wie der Server
    key = ftok("server.c", 65);

    if (key == -1)
    {
        perror("ftok");
        return 1;
    }

    // Vorhandenes Shared Memory holen
    shmid = shmget(key, sizeof(shared_data_t), 0666);

    if (shmid == -1)
    {
		printf("Kein Server gefunden. Bitte zuerst den Server starten.\n");
        //perror("shmget");
        return 1;
    }

    // Shared Memory anbinden
    shared_memory = (shared_data_t *) shmat(shmid, NULL, 0);

    if (shared_memory == (void *) -1)
    {
        perror("shmat");
        return 1;
    }

	//==========Semaphoren============
	key_t sem_key;
	int semid;

	sem_key = ftok("server.c", 66);
	if (sem_key == -1)
	{
		perror("ftok semaphore");
		return 1;
	}

	semid = semget(sem_key, 1, 0666);
	if (semid == -1)
	{
		printf("Keine Semaphore gefunden.\n");
		return 1;
	}

	//==========Eingabe============
	int running = 1;

	while(running)
	{
		int choice;
		char data[DATA_SIZE]; //lokaler Datenpuffer

		data[0] = '\0';

		printf("\nWas möchtest du tun?\n");
		printf("1 - PING\n");
		printf("2 - TEXT\n");
		printf("3 - LIST_ADD\n");
		printf("4 - LIST_SHOW\n");
		printf("5 - EXIT\n");
		printf("Auswahl: ");

		if (scanf("%d", &choice) != 1) //gibt durch %d bei integer eine 1 und sonst 0 zurück
		{
			printf("Ungültige Eingabe. Bitte eine Zahl eingeben.\n");

			// Ungültige Eingabe aus dem Eingabepuffer entfernen
			// würde sonst da alle Zeichen durchlaufen
			int c;
			while ((c = getchar()) != '\n' && c != EOF)
			{
			}

			continue;
		}


		//Eingabe vorbereiten
		switch (choice)
		{
			case 1:
				// PING braucht keine zusätzlichen Daten
				break;

			case 2:
				printf("Text eingeben: ");

				// Restliches '\n' von scanf entfernen - kommt durchs Enter drücken
				getchar();

				if (fgets(data, sizeof(data), stdin) == NULL)
				{
					printf("Fehler beim Einlesen.\n");
					continue;
				}
				// \n von fgets entfernen
				data[strcspn(data, "\n")] = '\0';

				break;

			case 3:
				printf("Text eingeben: ");

				// Restliches '\n' von scanf entfernen - kommt durchs Enter drücken
				getchar();

				if (fgets(data, sizeof(data), stdin) == NULL)
				{
					printf("Fehler beim Einlesen.\n");
					continue;
				}
				// \n von fgets entfernen
				data[strcspn(data, "\n")] = '\0';

				break;
			
			case 4:
				// LIST_SHOW braucht keine zusätzlichen Daten
				break;

			case 5:
				// EXIT braucht keine zusätzlichen Daten
				break;

			default:
				printf("Ungültige Auswahl.\n");
				continue;
		}

		// Ab hier beginnt der kritische Abschnitt
		printf("Warte auf Semaphore...\n");	
		sem_lock(semid); //Zack gelockt
		printf("Semaphore erhalten!\n");

		//Shared Memory bespielen
		shared_memory->pdu.type = choice;

    	strcpy(shared_memory->pdu.data, data);

		// Server informieren -> Startschuss
		shared_memory->status = 1;

		// Auf Antwort warten
		while (shared_memory->status != 2)
		{
			//schön Busywait
		}

		// Antwort lesen
		printf("Antwort vom Server: %s\n", shared_memory->pdu.data);

		// Kommunikation abgeschlossen
		shared_memory->status = 0;

		printf("Semaphore wird freigegeben.\n");
		sem_unlock(semid); //Zick unlock

		// Erst danach prüfen, ob Client beendet werden soll, damit man noch die Semaphore unlockt
		if (choice == 5)
			{
				running = 0;
				printf("Client wird beendet\n");
			}
		
	}

    // Shared Memory wieder lösen
    shmdt(shared_memory);

    return 0;
}