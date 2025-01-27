#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <stddef.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <time.h>

extern int errno;
#define PORT 1234
#define N 8500

typedef struct thData {
    int idThread, cl;
    xmlDocPtr fisier;
} thData;

static void *treat(void *arg);
void raspunde(void *arg);
xmlDocPtr CitesteXML(char *fis);
void ActualizareSosire(xmlDocPtr fisier, int nr_min, int id_tren);
void ActualizareIntarzierePlecare(xmlDocPtr fisier, int nr_min, int id_tren);
char *CalculeazaOraEstimata(char *ora_initiala, int intarziere_minute);
char *StatusTren(xmlDocPtr fisier, int id_tren);
char *MersulTrenurilor(xmlDocPtr fisier);
char *AfiseazaSosiriPlecari(xmlDocPtr fisier, char *oras, int keyword);

int logat;

int main() {
  	struct sockaddr_in server, from;
    int sd, i = 0;
  	int optval = 1;
    pthread_t th[100];

    xmlDocPtr fisier = CitesteXML("trenuri.xml");
    if(fisier == NULL) {
        perror("Eroare la incarcarea fisierului XML.\n");
        return errno;
    }

    if((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
      	perror("Eroare la socket in cadrul serverului.\n");
        return errno;
    }

    setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(int));
    bzero(&server, sizeof(server));

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    server.sin_port = htons(PORT);

    if(bind(sd, (struct sockaddr *)&server, sizeof(server)) == -1) {
      	perror("Eroare la bind in cadrul serverului.\n");
        return errno;
    }

    if(listen(sd, 5) == -1) {
      	perror("Eroare la listen in cadrul serverului.\n");
        return errno;
    }

    while(1) {
      	int client, lg;
        thData *td;
        lg = sizeof(from);

        printf("Asteptam la portul %d...\n", PORT);
    	fflush(stdout);

        client = accept(sd, (struct sockaddr *)&from, &lg);
        if(client < 0) {
          	perror("Eroare la accept in cadrul serverului.\n");
            continue;
        }

		td = (struct thData *)malloc(sizeof(struct thData));
        td->idThread = i++;
        td->cl = client;
        td->fisier = fisier;
        pthread_create(&th[i], NULL, &treat, td);
    }
    xmlFreeDoc(fisier);
    xmlCleanupParser();
  	return 0;
}

static void *treat(void *arg) {
    thData tdL;
    tdL = *((struct thData *)arg);
    printf("[thread]-%d- Asteptam mesajul...\n", tdL.idThread);
    fflush(stdout);
    pthread_detach(pthread_self());
    raspunde(arg);
    close(tdL.cl);
    return NULL;
}

void raspunde(void *arg) {
  	thData tdL;
    tdL = *((struct thData *)arg);
    char comanda[256], raspuns[N];
    xmlDocPtr fisier = tdL.fisier;

    while(1) {
        bzero(comanda, sizeof(comanda));
        bzero(raspuns, sizeof(raspuns));

        if(read(tdL.cl, comanda, sizeof(comanda)) <= 0) {
            printf("Eroare la read in cadrul thread-ului.\n");
            break;
        }

        printf("[thread]-%d- Am primit comanda: %s\n", tdL.idThread, comanda);

        if(strncmp(comanda, "status_tren ", 12) == 0) {
            int nr_tren;
            if(sscanf(comanda + 12, "%d", &nr_tren) == 1 && nr_tren > 0) {
                snprintf(raspuns, sizeof(raspuns), "%s", StatusTren(fisier, nr_tren));
            }
            else {
                strcpy(raspuns, "Sintaxa gresita, comanda corecta: status_tren <nr_tren>");
            }
        }
        else if(strcmp(comanda, "mersul_trenurilor") == 0) {
            snprintf(raspuns, sizeof(raspuns), "%s", MersulTrenurilor(fisier));
        }
        else if(strncmp(comanda, "login_admin", 11) == 0) {
            char parola[4];
            sscanf(comanda + 12, "%s", parola);
            if(strcmp(parola, "123") == 0) {
                logat = 1;
                strcpy(raspuns, "Accesare cont administrator reusita!");
            }
            else {
                strcpy(raspuns, "Parola incorecta!");
            }
        }
        else if(strncmp(comanda, "add_intarziere ", 15) == 0) {
            if(logat == 1) {
                int nr_min, nr_tren;
                if(sscanf(comanda + 15, "%d %d", &nr_min, &nr_tren) == 2 && nr_min > 0 && nr_tren > 0) {
                    ActualizareSosire(fisier, nr_min, nr_tren);
                    snprintf(raspuns, sizeof(raspuns), "Trenului %d i-a fost adaugata o intarziere de %d minute.", nr_tren, nr_min);
                }
                else {
                    strcpy(raspuns, "Sintaxa gresita, comanda corecta: add_intarziere <nr_min> <nr_tren>");
                }
            }
            else {
                strcpy(raspuns, "Eroare! Doar un administrator poate folosi aceasta coamnda.");
            }
        }
        else if(strncmp(comanda, "add_intarziere_plecare ", 23) == 0) {
            if(logat == 1) {
                int nr_min, nr_tren;
                if(sscanf(comanda + 23, "%d %d", &nr_min, &nr_tren) == 2 && nr_min > 0 && nr_tren > 0) {
                    ActualizareIntarzierePlecare(fisier, nr_min, nr_tren);
                    snprintf(raspuns, sizeof(raspuns), "Ora plecarii a trenului %d a fost intarziata cu %d minute.", nr_tren, nr_min);
                }
                else {
                    strcpy(raspuns, "Sintaxa gresita, comanda corecta: add_intarziere_plecare <nr_min> <nr_tren>");
                }
            }
            else {
                strcpy(raspuns, "Eroare! Doar un administrator poate folosi această comanda.");
            }
        }
        else if(strncmp(comanda, "add_mai_devreme ", 15) == 0) {
            if(logat == 1) {
                int nr_min, nr_tren;
                if(sscanf(comanda + 16, "%d %d", &nr_min, &nr_tren) == 2 && nr_min > 0 && nr_tren > 0) {
                    ActualizareSosire(fisier, -nr_min, nr_tren);
                    snprintf(raspuns, sizeof(raspuns), "Trenului %d i-a fost adaugata o sosire mai devreme cu %d minute.", nr_tren, nr_min);
                }
                else {
                    strcpy(raspuns, "Format invalid. Comanda corecta: add_mai_devreme <minute> <numar_tren>.");
                }
            }
            else {
                strcpy(raspuns, "Eroare! Doar un administrator poate folosi aceasta comanda.");
            }
        }
        else if(strcmp(comanda, "quit") == 0) {
            strcpy(raspuns, "Serverul devine inactiv.");
            write(tdL.cl, raspuns, strlen(raspuns));
            break;
        }
        else if(strcmp(comanda, "logout") == 0) {
            if(logat) {
                logat = 0;
                strcpy(raspuns, "Deconectare cont administrator reusita.");
            }
            else {
                strcpy(raspuns, "Eroare! Nu sunteti logat.");
            }
        }
        else if(strncmp(comanda, "plecari ", 8) == 0) {
            char oras[12];
            if(sscanf(comanda + 8, "%11s", oras) == 1) {
                snprintf(raspuns, sizeof(raspuns), "%s", AfiseazaSosiriPlecari(fisier, oras, 1));
            }
            else {
                strcpy(raspuns, "Format invalid. Comanda corecta: plecari <oras>.");
            }
        }
        else if(strncmp(comanda, "sosiri ", 7) == 0) {
            char oras[12];
            if(sscanf(comanda + 7, "%11s", oras) == 1) {
                snprintf(raspuns, sizeof(raspuns), "%s", AfiseazaSosiriPlecari(fisier, oras, 2));
            }
            else {
                strcpy(raspuns, "Format invalid. Comanda corecta: sosiri <oras>.");
            }
        }
        else {
            strcpy(raspuns, "Eroare! Comanda necunoscuta.");
        }
        write(tdL.cl, raspuns, strlen(raspuns));
    }
}

xmlDocPtr CitesteXML(char *file) {
    xmlDocPtr fisier = xmlReadFile(file, NULL, 0);
    if(fisier == NULL) {
        perror("Eroare la citirea fisierului XML.\n");
        return 0;
    }
    return fisier;
}

char *MersulTrenurilor(xmlDocPtr fisier) {
    xmlNodePtr root = xmlDocGetRootElement(fisier);
    xmlNodePtr tren = root->children;

    static char mersul[N];
    bzero(mersul, sizeof(mersul));
    strcat(mersul, "Trenuri disponibile:\n");

    while(tren) {
        if(tren->type == XML_ELEMENT_NODE) {
            xmlChar *id = xmlGetProp(tren, (xmlChar *)"id");
            if(id) {
                char *status = StatusTren(fisier, atoi((char *)id));
                if(status) {
                    strcat(mersul, status);
                    strcat(mersul, "\n");
                }
                xmlFree(id);
            }
        }
        tren = tren->next;
    }
    return mersul;
}

char *StatusTren(xmlDocPtr fisier, int id_tren) {
    xmlNodePtr root = xmlDocGetRootElement(fisier);
    xmlNodePtr tren = root->children;

    while(tren) {
        if(tren->type == XML_ELEMENT_NODE) {
            xmlChar *id = xmlGetProp(tren, (xmlChar *)"id");
            if(atoi((char *)id) == id_tren) {
                static char status [N];
                xmlNodePtr plecare = NULL, destinatie = NULL, ora_plecare = NULL, ora_sosire = NULL, intarziere_plecare = NULL, intarziere_sosire = NULL;

                xmlNodePtr currentNode = tren->children;
                while(currentNode) {
                    if(currentNode->type == XML_ELEMENT_NODE) {
                        if(strcmp((char *)currentNode->name, "plecare") == 0) { plecare = currentNode; }
                        else if(strcmp((char *)currentNode->name, "destinatie") == 0) { destinatie = currentNode; }
                        else if(strcmp((char *)currentNode->name, "ora_plecare") == 0) { ora_plecare = currentNode; }
                        else if(strcmp((char *)currentNode->name, "ora_sosire") == 0) { ora_sosire = currentNode; }
                        else if(strcmp((char *)currentNode->name, "intarziere_plecare") == 0) { intarziere_plecare = currentNode; }
                        else if(strcmp((char *)currentNode->name, "intarziere_sosire") == 0) { intarziere_sosire = currentNode; }
                    }
                    currentNode = currentNode->next;
                }

                if(plecare && destinatie && ora_plecare && ora_sosire && intarziere_plecare && intarziere_sosire) {
                    char *ora_plecare_estimata = CalculeazaOraEstimata((char *)xmlNodeGetContent(ora_plecare), atoi((char *)xmlNodeGetContent(intarziere_plecare)));
                    char *ora_sosire_estimata = CalculeazaOraEstimata((char *)xmlNodeGetContent(ora_sosire), atoi((char *)xmlNodeGetContent(intarziere_sosire)));

                    int intarziere_sosire_min = atoi((char *)xmlNodeGetContent(intarziere_sosire));
                    char estimare_sosire[100];

                    if(intarziere_sosire_min < 0) {
                        snprintf(estimare_sosire, sizeof(estimare_sosire), "Va sosi cu %d minute mai devreme (Estimare: %s)", -intarziere_sosire_min, ora_sosire_estimata);
                    }
                    else {
                        snprintf(estimare_sosire, sizeof(estimare_sosire), "Va sosi cu o intarziere de %d minute (Estimare: %s)", intarziere_sosire_min, ora_sosire_estimata);
                    }

                    snprintf(status, sizeof(status),
                             "Tren %d: Plecare din %s, Destinatie: %s, Ora plecare: %s (Intarziere: %s minute, Estimare: %s), Ora sosire: %s (%s).",
                             id_tren,
                             (char *)xmlNodeGetContent(plecare),
                             (char *)xmlNodeGetContent(destinatie),
                             (char *)xmlNodeGetContent(ora_plecare),
                             (char *)xmlNodeGetContent(intarziere_plecare),
                             ora_plecare_estimata,
                             (char *)xmlNodeGetContent(ora_sosire),
                             estimare_sosire);
                }
                xmlFree(id);
                return status;
            }
            xmlFree(id);
        }
        tren = tren->next;
    }
    return "Trenul cerut nu exista in baza de date.";
}

void ActualizareIntarzierePlecare(xmlDocPtr fisier, int nr_min, int id_tren) {
    xmlNodePtr root = xmlDocGetRootElement(fisier);
    xmlNodePtr tren = root->children;

    while(tren) {
        if(tren->type == XML_ELEMENT_NODE) {
            xmlChar *id = xmlGetProp(tren, (xmlChar *)"id");
            if(atoi((char *)id) == id_tren) {
                xmlNodePtr currentNode = tren->children;
                while(currentNode) {
                    if(currentNode->type == XML_ELEMENT_NODE && strcmp((char *)currentNode->name, "intarziere_plecare") == 0) {
                        int val_min = atoi((char *)xmlNodeGetContent(currentNode));
                        val_min += nr_min;

                        char actualizare_min[10];
                        sprintf(actualizare_min, "%d", val_min);
                        xmlNodeSetContent(currentNode, (xmlChar *)actualizare_min);
                        printf("Trenul %d: are acum o intarziere la plecare de %d minute.\n", id_tren, val_min);
                        break;
                    }
                    currentNode = currentNode->next;
                }
                xmlFree(id);
                break;
            }
            xmlFree(id);
        }
        tren = tren->next;
    }
    if(xmlSaveFile("trenuri.xml", fisier) == -1) {
        perror("Eroare la salvarea fisierului XML.\n");
    }
}

void ActualizareSosire(xmlDocPtr fisier, int nr_min, int id_tren) {
    xmlNodePtr root = xmlDocGetRootElement(fisier);
    xmlNodePtr tren = root->children;

    while(tren) {
        if(tren->type == XML_ELEMENT_NODE) {
            xmlChar *id = xmlGetProp(tren, (xmlChar *)"id");
            if(atoi((char *)id) == id_tren) {
                xmlNodePtr currentNode = tren->children;
                while(currentNode) {
                    if(currentNode->type == XML_ELEMENT_NODE && strcmp((char *)currentNode->name, "intarziere_sosire") == 0) {
                        int val_min = atoi((char *)xmlNodeGetContent(currentNode));
                        val_min += nr_min;

                        char actualizare_min[10];
                        sprintf(actualizare_min, "%d", val_min);
                        xmlNodeSetContent(currentNode, (xmlChar *)actualizare_min);

                        if(val_min < 0) {
                            printf("Trenului %d: i-a fost actualizata sosirea cu %d minute mai devreme.\n", id_tren, -val_min);
                        }
                        else {
                            printf("Trenul %d: soseste acum cu %d minute intarziere.\n", id_tren, val_min);
                        }
                        break;
                    }
                    currentNode = currentNode->next;
                }
                xmlFree(id);
                break;
            }
            xmlFree(id);
        }
        tren = tren->next;
    }
    if(xmlSaveFile("trenuri.xml", fisier) == -1) {
        perror("Eroare la salvarea fisierului XML.\n");
    }
}

char *AfiseazaSosiriPlecari(xmlDocPtr fisier, char *oras, int keyword) {
    xmlNodePtr root = xmlDocGetRootElement(fisier);
    xmlNodePtr tren = root->children;

    static char rez[N];
    bzero(rez, sizeof(rez));
    int gasit = 0;

    if(keyword == 1) {
        strcat(rez, "Trenuri care pleaca din ");
    }
    else { /// sau 2
        strcat(rez, "Trenuri care sosesc in ");
    }
    strcat(rez, oras);
    strcat(rez, " in urmatoarea ora:\n");

    time_t now = time(NULL);
    struct tm *ora_curenta = localtime(&now);
    int ora_curenta_min = ora_curenta->tm_hour * 60 + ora_curenta->tm_min;

    while(tren) {
        if(tren->type == XML_ELEMENT_NODE) {
            xmlNodePtr orasA = NULL, ora = NULL, intarziere = NULL, orasB = NULL;

            xmlNodePtr currentNode = tren->children;
            while(currentNode) {
                if(currentNode->type == XML_ELEMENT_NODE) {
                    if(keyword == 1) { ///plecari
                        if(strcmp((char *)currentNode->name, "plecare") == 0) { orasA = currentNode; }
                        else if(strcmp((char *)currentNode->name, "ora_plecare") == 0) { ora = currentNode; }
                        else if(strcmp((char *)currentNode->name, "intarziere_plecare") == 0) { intarziere = currentNode; }
                        else if(strcmp((char *)currentNode->name, "destinatie") == 0) { orasB = currentNode; }
                    }
                    else { /// sosiri
                        if(strcmp((char *)currentNode->name, "destinatie") == 0) { orasA = currentNode; }
                        else if(strcmp((char *)currentNode->name, "ora_sosire") == 0) { ora = currentNode; }
                        else if(strcmp((char *)currentNode->name, "intarziere_sosire") == 0) { intarziere = currentNode; }
                        else if(strcmp((char *)currentNode->name, "plecare") == 0) { orasB = currentNode; }
                    }
                }
                currentNode = currentNode->next;
            }

            if(orasA && ora && intarziere && orasB) {
                if(strcmp((char *)xmlNodeGetContent(orasA), oras) == 0) {
                    char *ora_estimata = CalculeazaOraEstimata((char *)xmlNodeGetContent(ora), atoi((char *)xmlNodeGetContent(intarziere)));

                    if(strcmp(ora_estimata, "Eroare") != 0) {
                        struct tm estimare = {0};
                        if(strptime(ora_estimata, "%H:%M", &estimare)) {
                            int ora_estimata_minute = estimare.tm_hour * 60 + estimare.tm_min;

                            int diferenta_min = ora_estimata_minute - ora_curenta_min;
                            if(diferenta_min < 0) {
                                diferenta_min += 1440; /// in cazul in care e ora 23:00, iar trenul pleaca la 00:00
                            }

                            if(diferenta_min >= 0 && diferenta_min <= 60) {
                                char msj_afisare[N];
                                gasit = 1;
                                snprintf(msj_afisare, sizeof(msj_afisare),
                                         "ID: %s, %s la %s (Intarziere de: %s minute, Estimare: %s), %s: %s.\n",
                                         (char *)xmlGetProp(tren, (xmlChar *)"id"),
                                         keyword == 1 ? "Plecare" : "Sosire",
                                         (char *)xmlNodeGetContent(ora),
                                         (char *)xmlNodeGetContent(intarziere),
                                         ora_estimata,
                                         keyword == 1 ? "Destinatie" : "Plecare din",
                                         (char *)xmlNodeGetContent(orasB));
                                strcat(rez, msj_afisare);
                            }
                        }
                    }
                    free(ora_estimata);
                }
            }
        }
        tren = tren->next;
    }
    if(gasit == 0) {
        snprintf(rez, sizeof(rez), "Nu sunt trenuri care %s în %s in urmatoarea ora.\n", keyword == 1 ? "pleaca" : "sosesc", oras);
    }
    return rez;
}

char *CalculeazaOraEstimata(char *ora_initiala, int intarziere_minute) {
    char *ora_estimata = (char *)malloc(6 * sizeof(char)); /// aloc dinamic pt formatul "HH:MM\0"
    struct tm ora = {0};
    memset(&ora, 0, sizeof(ora));

    if (strptime(ora_initiala, "%H:%M", &ora) == NULL) {
        snprintf(ora_estimata, 6, "Eroare");
        return ora_estimata;
    }

    /// atribuim niste valori random pentru a evita erorile din mktime
    ora.tm_year = 100;
    ora.tm_mday = 1;

    /// convertim la time_t pt lucrul cu timpul in secunde
    time_t timp = mktime(&ora);
    if (timp == -1) {
        snprintf(ora_estimata, 6, "Eroare");
        return ora_estimata;
    }

    timp += intarziere_minute * 60;

    /// transformam inapoi in tm
    struct tm *estimare = localtime(&timp);
    if (estimare == NULL) {
        snprintf(ora_estimata, 6, "Eroare");
        return ora_estimata;
    }

    /// formatam in tipul HH:MM
    if (strftime(ora_estimata, 6, "%H:%M", estimare) == 0) {
        snprintf(ora_estimata, 6, "Eroare");
        return ora_estimata;
    }
    return ora_estimata;
}
