all:
	gcc -o client client.c
	gcc -o server server.c -lxml2 -lpthread
