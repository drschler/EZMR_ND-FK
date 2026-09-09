#ifndef PROTOCOL_H
#define PROTOCOL_H

typedef struct
{
    volatile int status;
	//ohne volatile weiß der compiler nicht unbedingt, dass ein anderer Prozess den Speicher geändert hat und kommt nicht aus dem Busy-Wait
	//jetzt wird er bei jeder Prüfung neu aus dem Speicher gelesen
    int type;
    char data[256];
} pdu_t;

#endif