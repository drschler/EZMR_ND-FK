CC = gcc
CFLAGS = -Wall -Wextra -O2
TARGETS = server_prozess

all: $(TARGETS)

server_prozess: server_prozess.c
	$(CC) $(CFLAGS) -o server_prozess server_prozess.c

clean:
	rm -f $(TARGETS)
