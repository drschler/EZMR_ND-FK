CC = gcc
CFLAGS = -Wall -Wextra -O2
TARGETS = server server_list client peer peer_list

all: $(TARGETS)

server: server.c
	$(CC) $(CFLAGS) -o server server.c

server_list: server_list.c
	$(CC) $(CFLAGS) -o server_list server_list.c

client: client.c
	$(CC) $(CFLAGS) -o client client.c

peer: peer.c
	$(CC) $(CFLAGS) -o peer peer.c

peer_list: peer_list.c
	$(CC) $(CFLAGS) -o peer_list peer_list.c

clean:
	rm -f $(TARGETS)
