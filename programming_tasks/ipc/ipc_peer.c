#include <sys/sem.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <string.h>
#include <unistd.h>

#define MAX_ENTRIES 10
#define DATA_SIZE 256
#define MAX_MESSAGES 10

typedef struct
{
    int sender;
    char data[DATA_SIZE];
} message_t;

typedef struct
{
    message_t messages[MAX_MESSAGES];
    int message_count;

    char list[MAX_ENTRIES][DATA_SIZE];
    int list_count;

} shared_data_t;

void sem_lock(int semid)
{
    struct sembuf op;

    op.sem_num = 0;
    op.sem_op = -1;
    op.sem_flg = 0;

    if (semop(semid, &op, 1) == -1)
    {
        perror("semop lock");
        exit(EXIT_FAILURE);
    }
}

void sem_unlock(int semid)
{
    struct sembuf op;

    op.sem_num = 0;
    op.sem_op = 1;
    op.sem_flg = 0;

    if (semop(semid, &op, 1) == -1)
    {
        perror("semop unlock");
        exit(EXIT_FAILURE);
    }
}

int main()
{
    //Vorbau
    key_t sha_key;
    int shmid;
    shared_data_t *shared_memory;

	key_t sem_key;
	int semid;

    int first_peer = 0;

    
    sha_key = ftok("ipc_peer.c", 65);
    if (sha_key == -1)
    {
        perror("ftok");
        return 1;
    }

    sem_key = ftok("ipc_peer.c", 66);
	if (sem_key == -1)
	{
		perror("ftok semaphore");
		return 1;
	}

    shmid = shmget(sha_key, sizeof(shared_data_t), 0666 | IPC_CREAT);
    if (shmid == -1)
    {
        perror("shmget");
        return 1;
    }

    semid = semget(sem_key, 1, 0666 | IPC_CREAT | IPC_EXCL);
    if (semid != -1)
    {
        // Semaphore wurde gerade NEU erstellt
        first_peer = 1;

        semctl(semid, 0, SETVAL, 1);
        if (semctl(semid, 0, SETVAL, 1) == -1)
        {
            perror("semctl");
            return 1;
        }
    }
    else
    {
        // Semaphore existiert wahrscheinlich schon
        semid = semget(sem_key, 1, 0666);
        if (semid == -1)
        {
            perror("semget");
            exit(EXIT_FAILURE);
        }
    }

    shared_memory = (shared_data_t *) shmat(shmid, NULL, 0);
    if (shared_memory == (void *) -1)
    {
        perror("shmat");
        return 1;
    }

    if (first_peer)
    {
        shared_memory->message_count = 0;
        shared_memory->list_count = 0;
    }

    //KOMMUNIKATIONSBEREICH
    int peer_id = getpid();
    int running = 1;

    while (running)
    {
        int choice;
        char data[DATA_SIZE];

        data[0] = '\0';

        printf("\nWas möchtest du tun? (du bist - Peer%d)\n", peer_id);
        printf("1 - MESSAGE_ADD\n");
        printf("2 - MESSAGE_SHOW\n");
        printf("3 - LIST_ADD\n");
        printf("4 - LIST_SHOW\n");
        printf("5 - EXIT\n");
        printf("Auswahl: ");

        if (scanf("%d", &choice) != 1)
        {
            printf("Ungültige Eingabe.\n");

            int c;
            while ((c = getchar()) != '\n' && c != EOF)
            {
            }

            continue;
        }

        switch (choice)
        {
            case 1:
                printf("Nachricht eingeben: ");
                getchar();

                if (fgets(data, sizeof(data), stdin) == NULL)
                {
                    printf("Fehler beim Einlesen.\n");
                    continue;
                }

                data[strcspn(data, "\n")] = '\0';
                break;

            case 2:
                // keine zusätzliche Eingabe nötig
                break;

            case 3:
                printf("Listeneintrag eingeben: ");
                getchar();

                if (fgets(data, sizeof(data), stdin) == NULL)
                {
                    printf("Fehler beim Einlesen.\n");
                    continue;
                }

                data[strcspn(data, "\n")] = '\0';
                break;

            case 4:
                // keine zusätzliche Eingabe nötig
                break;

            case 5:
                // keine zusätzliche Eingabe nötig
                break;

            default:
                printf("Ungültige Auswahl.\n");
                continue;
        }

        sem_lock(semid);

        switch (choice)
        {
            case 1:
                // MESSAGE_ADD
                if (shared_memory->message_count < MAX_MESSAGES)
                {
                    shared_memory->messages[shared_memory->message_count].sender = peer_id;

                    strcpy(shared_memory->messages[shared_memory->message_count].data, data);

                    shared_memory->message_count++;

                    printf("Nachricht wurde gespeichert.\n");
                }
                else
                {
                    printf("Nachrichtenspeicher ist voll.\n");
                }

                break;

            case 2:
                // MESSAGE_SHOW
                if (shared_memory->message_count == 0)
                {
                    printf("Noch keine Nachrichten vorhanden.\n");
                }
                else
                {
                    printf("\n--- Nachrichten ---\n");

                    for (int i = 0; i < shared_memory->message_count; i++)
                    {
                        printf(
                            "Peer %d: %s\n",
                            shared_memory->messages[i].sender,
                            shared_memory->messages[i].data
                        );
                    }
                }

                break;

            case 3:
                // LIST_ADD
                if (shared_memory->list_count < MAX_ENTRIES)
                {

                    strcpy(
                        shared_memory->list[shared_memory->list_count],
                        data
                    );

                    shared_memory->list_count++;

                    printf("Listeneintrag wurde gespeichert.\n");
                }
                else
                {
                    printf("Liste ist voll.\n");
                }

                break;

            case 4:
                // LIST_SHOW
                if (shared_memory->list_count == 0)
                {
                    printf("Noch keine Listeneinträge vorhanden.\n");
                }
                else
                {
                    printf("\n--- Liste ---\n");

                    for (int i = 0; i < shared_memory->list_count; i++)
                    {
                        printf(
                            "Eintrag %d: %s\n",
                            i,
                            shared_memory->list[i]
                        );
                    }
                }
                break;

            case 5:
                running = 0;
                break;
        }

        sem_unlock(semid);
    }


    //sauberes AUFRÄUMEN
    struct shmid_ds shm_info;

    // Cleanup-Bereich schützen - falls zwei Peers gleichzeitig beenden sollten
    sem_lock(semid);

    // Anzahl aktuell angehängter Prozesse abfragen
    if (shmctl(shmid, IPC_STAT, &shm_info) == -1)
    {
        perror("shmctl IPC_STAT");

        sem_unlock(semid);
        shmdt(shared_memory);

        return 1;
    }

    printf("Aktuell verbundene Peers: %ld\n",
        (long)shm_info.shm_nattch);


    // Sind wir der letzte Peer?
    if (shm_info.shm_nattch == 1)
    {
        printf("Letzter Peer - IPC-Ressourcen werden entfernt.\n");

        // Semaphore erstmal wieder freigeben
        sem_unlock(semid);

        // Eigene Verbindung zum Shared Memory lösen
        if (shmdt(shared_memory) == -1)
        {
            perror("shmdt");
        }

        // Shared Memory entfernen
        if (shmctl(shmid, IPC_RMID, NULL) == -1)
        {
            perror("shmctl IPC_RMID");
        }

        // Semaphore entfernen
        if (semctl(semid, 0, IPC_RMID) == -1)
        {
            perror("semctl IPC_RMID");
        }
    }
    else
    {
        printf("Weitere Peers aktiv - eigene Verbindung wird getrennt.\n");

        // Kritischen Bereich wieder freigeben
        sem_unlock(semid);

        // Nur eigenen Shared-Memory-Anhang lösen
        if (shmdt(shared_memory) == -1)
        {
            perror("shmdt");
        }
    }

    return 0;
}