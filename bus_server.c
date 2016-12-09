#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <string.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>

#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_RESET   "\x1b[0m"

#define BUF_SIZE 1024

int server; /* socket du serveur en attente de connection */
char * sockname; /* nom de la socket */

static inline int max(int x, int y) {
    return x > y ? x : y;
}

void hdlr_fin(int sig) {
    printf("Reception du signal %d. Arret du bus !\n", sig);
    close(server);
    unlink(sockname);
    exit(EXIT_SUCCESS);
}

int main(int argc, char **argv) {
    struct sockaddr_un a;
    int * clients; /* sockets des clients */
    int max_clients; /* nombre de clients */
    char buffer[BUF_SIZE];
    int r, i, j;
    ssize_t rd_sz, wr_sz;

    if (argc != 3) {
        fprintf(stderr,
                "Utilisation:\n\t%s <Unix socket path> <max clients>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    /*
     * The SIGPIPE signal will be received if the peer has gone away
     * and an attempt is made to write data to the peer. Ignoring this
     * signal causes the write operation to receive an EPIPE error.
     * Thus, the user is informed about what happened.
     */
    signal(SIGPIPE, SIG_IGN);

    /* pour une terminaison propre sur QUIT TERM INT SEGV... */
    signal(SIGQUIT, hdlr_fin);
    signal(SIGTERM, hdlr_fin);
    signal(SIGINT, hdlr_fin);
    signal(SIGSEGV, hdlr_fin);

    max_clients = atoi(argv[2]);
    clients = (int *) malloc(max_clients * sizeof (int));
    for (i = 0; i < max_clients; i++) /* initialisation de cette structure */
        clients[i] = -1; /* convention choisie  -1 = pas de client */
    printf("%d client entries init\n", max_clients);

    /* creation de la socket serveur */
    if ((server = socket(PF_UNIX, SOCK_SEQPACKET, 0)) < 0) {
        perror("socket()");
        exit(EXIT_FAILURE);
    }
    printf("Socket init\n");

    /* preparation de la structure d'adresse de la socket serveur sur 
       laquelle on attend les connexions */
    memset(&a, 0, sizeof (a));
    a.sun_family = AF_UNIX;
    sockname = strncpy(a.sun_path, (char *) argv[1], sizeof (a.sun_path) - 1);

    if (bind(server, (struct sockaddr *) &a, sizeof (struct sockaddr_un)) < 0) {
        perror("bind()");
        close(server);
        exit(EXIT_FAILURE);
    }
    printf("Socket bind\n");

    if (listen(server, max_clients) < 0) {
        perror("listen()");
        close(server);
        exit(EXIT_FAILURE);
    }
    printf("Socket listening\n");

    /* le corps du programme */
    printf("Demarrage du bus.\n");
    for (;;) {
        printf("-------\nLoop start\n");
        int nfds = 0;
        fd_set rd_set; /* Catalogue des sockets interessantes en lecture */
        //fd_set wd_set;
        //fd_set ed_set;

        /* Nettoyage de ce catalogue */
        FD_ZERO(&rd_set);

        /* On y place les descripteurs interessants en lecture */
        /* On s'interesse a la socket du serveur pour d'eventuelles connexions */
        FD_SET(server, &rd_set);
        //FD_SET(STDIN_FILENO, &rd_set); //TEST

        nfds = max(nfds, server);
        //printf("DEBUG: before client loop nfds %d, server %d\n", nfds, server);
        /* ajoute les sockets des clients */
        for (i = 0; i < max_clients; i++) {
            if (clients[i] > 0) {
                FD_SET(clients[i], &rd_set);
                nfds = max(nfds, clients[i]);
            }
        }
        //printf("DEBUG: after client loop nfds %d, server %d\n", nfds, server);
        struct timeval tv;
        /* Attends jusqu’à 15 secondes. */
        tv.tv_sec = 15;
        tv.tv_usec = 0;

        /* Se bloque en attente de quelque chose d'interessant sur une socket */
        r = select(nfds+1, &rd_set, NULL, NULL, &tv);
        printf("Select set\n");

        if (r == -1 && errno == EINTR) {
            continue;
        }
        if (r < 0) {
            perror("select()");
            exit(EXIT_FAILURE);
        }
        if (r == 0) {
            printf("No data since last 15 seconds\n");
        }

        struct sockaddr_storage from;
        socklen_t fromlen = sizeof from;

        //TEST
        //wr_sz = send(server, "test", sizeof("test"), 0); 

        if (FD_ISSET(server, &rd_set)) { /* on a une nouvelle connection */ //TEST
            printf(ANSI_COLOR_GREEN "New connection" ANSI_COLOR_RESET "\n");
            r = accept(server, (struct sockaddr *) &from, &fromlen);
            if (r < 0) {
                perror("accept()");
            } else {
                /* on recherche un emplacement de libre dans notre liste de clients */
                for (i = 0; (i < max_clients) && (clients[i] > 0); i++);
                if (i < max_clients) {
                    printf("Un nouveau client %d se connecte.\n", i);
                    /* on y ajoute la socket du client */
                    clients[i] = r;
                } else {
                    printf("Plus de place pour un nouveau client.\n");
                    close(r);
                }
            }
        }

        /* Traite les lectures possibles */
        /* Pour chaque donnee lue, on la renvoie a tous les autres */
        for (i = 0; i < max_clients; i++) {
            if ((clients[i] > 0) && FD_ISSET(clients[i], &rd_set)) {
                printf("Read loop data from client %d\n", i);
                /* Hypothese : on lit le paquet d'un seul coup !
                   Prevoir une buffer assez grand */
                rd_sz = recv(clients[i], buffer, BUF_SIZE, MSG_WAITALL);
                if (rd_sz < 0) {
                    perror("recv()");
                    fprintf(stderr, "...probleme avec le client %d\n", i);
                    shutdown(clients[i], SHUT_RDWR);
                    clients[i] = -1;
                } else if (rd_sz == 0) {
                    printf("Le client %d et partit.\n", i);
                    close(clients[i]);
                    clients[i] = -1;
                } else if (rd_sz > 0) {
                    printf("Reception de %d octets du client %d : [" ANSI_COLOR_BLUE "\n", rd_sz, i);
                    /* On ecrit sur la sortie standard */
                    write(STDOUT_FILENO, buffer, rd_sz);
                    printf(ANSI_COLOR_RESET "]\n");
                    /* Envoie le paquet (en un seul coup) a tous les autres clients */
                    for (j = 0; j < max_clients; j++) {
                        if ((clients[j] > 0) && (i != j)) {
                            wr_sz = send(clients[j], buffer, rd_sz, 0);
                            if (wr_sz < 0) {
                                /* cloture du client j */
                                perror("send()");
                                fprintf(stderr, "...probleme avec le client %d\n", j);
                                shutdown(clients[j], SHUT_RDWR);
                                clients[j] = -1;
                            } else if (wr_sz == 0) {
                                printf("Le client %d et partit.\n", j);
                                close(clients[j]);
                                clients[j] = -1;
                            } else
                                printf("Envoie de %d octets au client %d.\n", wr_sz, j);
                        }
                    }
                }
            }
        }

    }
    /* superflu apres la boucle infinie */
    exit(EXIT_SUCCESS);
}
