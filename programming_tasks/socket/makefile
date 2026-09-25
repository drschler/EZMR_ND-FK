CC = gcc
CFLAGS = -Wall -Wextra -O2
TARGETS = server server_list client client_list peer peer_einzel peer_list

all: $(TARGETS)

server: server.c
	$(CC) $(CFLAGS) -o server server.c

server_list: server_list.c
	$(CC) $(CFLAGS) -o server_list server_list.c

client: client.c
	$(CC) $(CFLAGS) -o client client.c

client_list: client_list.c
	$(CC) $(CFLAGS) -o client_list client_list.c

peer_einzel: peer_einzel.c
	$(CC) $(CFLAGS) -o peer_einzel peer_einzel.c

peer: peer.c
	$(CC) $(CFLAGS) -o peer peer.c

peer_list: peer_list.c
	$(CC) $(CFLAGS) -o peer_list peer_list.c

clean:
	rm -f $(TARGETS)
