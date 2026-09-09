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

    // 1. Schlüssel erzeugen
    key = ftok("server.c", 65);

    if (key == -1)
    {
        perror("ftok");
        return 1;
    }

    // 2. Shared Memory anlegen
    shmid = shmget(key, sizeof(pdu_t), 0666 | IPC_CREAT);

    if (shmid == -1)
    {
        perror("shmget");
        return 1;
    }

    // 3. Shared Memory an diesen Prozess anbinden
    shared_memory = (pdu_t *) shmat(shmid, NULL, 0);
    
    if (shared_memory == (void *) -1)
    {
        perror("shmat");
        return 1;
    }

    //Anfangszustand definieren
    shared_memory->status = 0;
    shared_memory->type = 0;
    shared_memory->data[0] = '\0';

    printf("Server gestartet.\n");
    printf("Warte auf Anfragen...\n");

    int running = 1;

    while (running)
    {
        while (shared_memory->status != 1)
        {
            // Busy Waiting:
            // Warten, bis der Client eine Anfrage geschrieben hat
        }

        printf("Anfrage empfangen.\n");

        // Anfrage auswerten
        switch (shared_memory->type)
        {
            case 1:
                printf("PING empfangen.\n");
                strcpy(shared_memory->data, "PONG");
                break;

            case 2:
                printf("TEXT empfangen: %s\n", shared_memory->data);
                strcpy(shared_memory->data, "Text wurde empfangen.");
                break;

            case 3:
                printf("EXIT empfangen.\n");
                strcpy(shared_memory->data, "Server wird beendet.");
                running = 0;
                break;

            default:
                printf("Unbekannter Typ: %d\n", shared_memory->type);
                strcpy(shared_memory->data, "Unbekannter Befehl.");
                break;
        }

        // Dem Client sagen:
        // Antwort ist fertig
        shared_memory->status = 2;
    }

    // sauber shared Memory abschließen
    shmdt(shared_memory);
    shmctl(shmid, IPC_RMID, NULL);

    return 0;
}