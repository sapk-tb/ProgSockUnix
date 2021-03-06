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

static inline int max(int x, int y) {
    return x > y ? x : y;
}

void hdlr_fin(int sig) {
    printf("Reception du signal %d. Arret du client !\n", sig);
    close(server);
    exit(EXIT_SUCCESS);
}

int main(int argc, char **argv) {
    struct sockaddr_un a;
    char buffer[BUF_SIZE];
    int r;
    ssize_t rd_sz, wr_sz;

    if (argc != 2) {
        fprintf(stderr,
                "Utilisation:\n\t%s <Unix socket path>\n", argv[0]);
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

    /* creation de la socket client */
    if ((server = socket(PF_UNIX, SOCK_SEQPACKET, 0)) < 0) {
        perror("socket()");
        exit(EXIT_FAILURE);
    }

    /* preparation de la structure d'adresse de la socket serveur sur 
       laquelle on va se connecter */
    memset(&a, 0, sizeof (a)); /* nettoyage de la structure */
    a.sun_family = AF_UNIX; /* famille de l'adresse */
    strncpy(a.sun_path, argv[1], sizeof (a.sun_path) - 1 /* 108 UNIX_PATH_MAX*/);

    /* Connexion au serveur */
    if (connect(server, (struct sockaddr *) &a, sizeof (a)) < 0) {
        perror("connect()");
        close(server);
        exit(EXIT_FAILURE);
    }

    /* le corps du programme */
    printf(ANSI_COLOR_GREEN "Le client est accroche au bus." ANSI_COLOR_RESET "\n");
    for (;;) {
        int nfds = 0;
        fd_set rd_set; /* Cree le catalogue des sockets interessantes en lecture */
        //fd_set wd_set;
        //fd_set ed_set;

        /* Nettoyage de ce catalogue */
        FD_ZERO(&rd_set);

        /* On y place les descripteurs interessants en lecture */
        /* On s'interesse a la socket du serveur */
        FD_SET(server, &rd_set);
        nfds = max(nfds, server);
        /* ajoute STDIN */
        FD_SET(STDIN_FILENO, &rd_set);
        nfds = max(nfds, STDIN_FILENO); /* pour le principe */

        struct timeval tv;
        /* Attends jusqu’à 5 secondes. */
        tv.tv_sec = 5;
        tv.tv_usec = 0;
        /* Se bloque en attente de quelque chose d'interessant */
        r = select(nfds + 1, &rd_set, NULL, NULL, &tv);

        if (r == -1 && errno == EINTR)
            continue;
        if (r < 0) {
            perror("select()");
            exit(EXIT_FAILURE);
        }

        if (FD_ISSET(server, &rd_set)) { /* evenement socket */
            /* Hypothese : on lit le paquet d'un seul coup !
               Prevoir une buffer assez grand */
            rd_sz = recv(server, buffer, BUF_SIZE, MSG_WAITALL);
            if (rd_sz < 0) {
                perror("recv()");
                close(server);
                exit(EXIT_FAILURE);
            } else if (rd_sz == 0) {
                printf(ANSI_COLOR_RED "Le bus est arrete !" ANSI_COLOR_RESET "\n");
                exit(EXIT_SUCCESS);
            } else if (rd_sz > 0) {
                printf("Reception de %d octets : [" ANSI_COLOR_BLUE "\n", rd_sz);
                /* Ecriture sur le staandard de sortie de ce qui est recu du serveur */
                write(STDOUT_FILENO, buffer, rd_sz);
                printf(ANSI_COLOR_RESET "]\n");
            }
        }

        if (FD_ISSET(STDIN_FILENO, &rd_set)) { /* evenement stdin */
            /* Hypothese : on lit le paquet d'un seul coup !
               Prevoir une buffer assez grand */
            fgets(buffer, BUF_SIZE, stdin);
            rd_sz = strlen(buffer);
            /* envoi vers le serveur */
            wr_sz = send(server, buffer, rd_sz, 0);
            if (wr_sz < 0) {
                perror("send()");
                close(server);
                exit(EXIT_FAILURE);
            } else if (wr_sz == 0) {
                printf(ANSI_COLOR_RED "Le bus est arrete !" ANSI_COLOR_RESET "\n");
                exit(EXIT_SUCCESS);
            } else if (wr_sz > 0) {
                printf(ANSI_COLOR_GREEN "Envoie de %d octets" ANSI_COLOR_RESET "\n", wr_sz);
            }
        }

    }
    /* superflu apres la boucle infinie */
    exit(EXIT_SUCCESS);
}
