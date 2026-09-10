#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <string.h>
#include <unistd.h>

#include "header_list.h"

int main()
{
    // ==== Shared Memory ====

    key_t key;
    int shmid;
    shared_data_t *shared_memory;
    
    // Schlüssel
    key = ftok("server.c", 65);

    if (key == -1)
    {
        perror("ftok");
        return 1;
    }

    // ID
    shmid = shmget(key, sizeof(shared_data_t), 0666 | IPC_CREAT);

    if (shmid == -1)
    {
        perror("shmget");
        return 1;
    }

    // an Prozess anbinden
    shared_memory = (shared_data_t *) shmat(shmid, NULL, 0);
    
    if (shared_memory == (void *) -1)
    {
        perror("shmat");
        return 1;
    }

    //Anfangszustand definieren
    shared_memory->status = 0;
    shared_memory->pdu.type = 0;
    shared_memory->pdu.data[0] = '\0';
    shared_memory->list_count = 0;

    // ==== Semaphoren ====
    
    key_t sem_key;
    int semid;

    // Schlüssel
    sem_key = ftok("server.c", 66);

    if (sem_key == -1)
    {
        perror("ftok semaphore");
        return 1;
    }

    // ID
    semid = semget(sem_key, 1, 0666 | IPC_CREAT);

    if (semid == -1)
    {
        perror("semget");
        return 1;
    }

    // Initialisieren
    semctl(semid, 0, SETVAL, 1); //auf 1, also bereit um das Ding zu locken

    //=====


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

        //sleep(5); //EXTRAVERZÖGERUNG zum Test der Semaphore

        // Anfrage auswerten
        switch (shared_memory->pdu.type)
        {
            case 1:
                printf("PING empfangen.\n");
                strcpy(shared_memory->pdu.data, "PONG");
                break;

            case 2:
                printf("TEXT empfangen: %s\n", shared_memory->pdu.data);
                strcpy(shared_memory->pdu.data, "Text wurde empfangen.");
                break;

            case 3:
                printf("LIST_ADD empfangen: %s\n", shared_memory->pdu.data);
                if (shared_memory->list_count < MAX_ENTRIES)
                {
                    strcpy(shared_memory->list[shared_memory->list_count], shared_memory->pdu.data);
                    snprintf(shared_memory->pdu.data, sizeof(shared_memory->pdu.data), "Eintrag wurde hinzugefuegt (list_count = %d).", shared_memory->list_count);
                    shared_memory->list_count++;
                }
                else
                {
                    strcpy(shared_memory->pdu.data, "Liste ist voll.");
                }
                break;
                
case 4:
    printf("LIST_SHOW empfangen.\n");

    // erstmal leeren
    shared_memory->pdu.data[0] = '\0';

    if (shared_memory->list_count == 0)
    {
        strcpy(shared_memory->pdu.data,
               "Die Liste ist leer.");
    }
    else
    {
        for (int i = 0; i < shared_memory->list_count; i++)
        {
            //temporärer String
            char entry[300];

            snprintf(entry, sizeof(entry), "%d: %s\n", i + 1, shared_memory->list[i]);
            strcat(shared_memory->pdu.data, entry);
        }
    }

    break;

            case 5:
                printf("EXIT empfangen.\n");
                strcpy(shared_memory->pdu.data, "Server wird beendet.");
                running = 0;
                break;

            default:
                printf("Unbekannter Typ: %d\n", shared_memory->pdu.type);
                strcpy(shared_memory->pdu.data, "Unbekannter Befehl.");
                break;
        }

        // Dem Client sagen:
        // Antwort ist fertig
        shared_memory->status = 2;
    }

    // IPC Ressourcen aufräumen
    shmdt(shared_memory);
    shmctl(shmid, IPC_RMID, NULL);

    semctl(semid, 0, IPC_RMID);

    return 0;
}