all: Server Klient
Server : server.c network.c
	gcc -std=gnu99 server.c -o server
Klient : klient.c network.c
	gcc -std=gnu99 klient.c -o klient
