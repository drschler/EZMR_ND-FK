#include <sys/sem.h>

#ifndef PROTOCOL_H
#define PROTOCOL_H

#define MAX_ENTRIES 10
#define DATA_SIZE 256

//PDU = Nachricht
typedef struct
{
    int type;
    char data[DATA_SIZE];
} pdu_t;

//Shared Memory = Kommunikationsbereich + Daten
typedef struct
{
    volatile int status;

    pdu_t pdu;

    char list[MAX_ENTRIES][DATA_SIZE];
    int list_count;

} shared_data_t;

//Semaphor-Funktionen
void sem_lock(int semid)
{
    struct sembuf op;

    op.sem_num = 0;
    op.sem_op = -1; //für Lock: -1 -> das geht nur wenn Sie vorher auf 1 ist -> wenn vorher schon einer gelockt hat ist sie ja schon auf 0 und kann nicht auf -1 gehen
    op.sem_flg = 0;

    semop(semid, &op, 1);
}

void sem_unlock(int semid)
{
    struct sembuf op;

    op.sem_num = 0;
    op.sem_op = 1; //unlock: auf 1 -> wieder frei zum locken
    op.sem_flg = 0;

    semop(semid, &op, 1);
}
#endif