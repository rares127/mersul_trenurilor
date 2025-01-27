#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>

extern int errno;
int port;

int main(int argc, char *argv[]) {
    int sd;
    struct sockaddr_in server;
    char comanda[256], raspuns[8500];

    if(argc != 3) {
      printf("Sintaxa este: %s <adresa_server> <port>\n", argv[0]);
      return -1;
    }

    port = atoi(argv[2]);

    if((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Eroare la socket in cadrul clientului.\n");
        return errno;
    }

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr(argv[1]);
    server.sin_port = htons(port);

    if(connect(sd, (struct sockaddr *)&server, sizeof(server)) == -1) {
        perror("Eroare la connect in cadrul clientului.\n");
        close(sd);
        return errno;
    }

    while(1) {
        printf("\nIntroduceti comanda: (status_tren, mersul_trenurilor, login_admin, add_intarziere_plecare, add_intarziere, add_mai_devreme, plecari, sosiri, logout, quit).\n\n");
        bzero(comanda, 256);
        fgets(comanda, 256, stdin);

        if(comanda[strlen(comanda) - 1] == '\n') {
            comanda[strlen(comanda) - 1] = '\0';
        }

        if(write(sd, comanda, strlen(comanda)) <= 0) {
            perror("Eroare la write in cadrul clientului.\n");
            close(sd);
            return errno;
        }

        if(strcmp(comanda, "quit") == 0) {
            printf("Clientul a fost inchis.\n");
            break;
        }

        bzero(raspuns, sizeof(raspuns));
        if(read(sd, raspuns, sizeof(raspuns)) <= 0) {
            perror("Eroare la read in cadrul clientului.\n");
            close(sd);
            return errno;
        }
        raspuns[strlen(raspuns) - 1] = '\0';
        printf("Raspuns de la server: %s\n", raspuns);
    }
    close(sd);
    return 0;
}
