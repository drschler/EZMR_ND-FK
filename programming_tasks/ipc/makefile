CC = gcc
CFLAGS = -Wall -Wextra -O2
TARGETS = server client server_list client_list ipc_peer

all: $(TARGETS)

server: server.c
	$(CC) $(CFLAGS) -o server server.c

client: client.c
	$(CC) $(CFLAGS) -o client client.c

server_list: server_list.c
	$(CC) $(CFLAGS) -o server_list server_list.c

client_list: client_list.c
	$(CC) $(CFLAGS) -o client_list client_list.c

ipc_peer: ipc_peer.c
	$(CC) $(CFLAGS) -o ipc_peer ipc_peer.c

clean:
	rm -f $(TARGETS)
