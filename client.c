#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>

#define BUFSIZE 512
int sfd;

void hdlr_fin(int sig) {
    printf("Reception du signal %d. Arret du client !\n", sig);
    close(sfd);
    exit(EXIT_SUCCESS);
}

int main(int argc, char **argv) {
    int s, r;
    struct sockaddr_un a;
    struct addrinfo *result, *rp;
    char buf[BUFSIZE], *pt;
    ssize_t nread, nwrite;

    if (argc != 2) {
        printf("Usage: %s socket\n", argv[0]);
        exit(EXIT_FAILURE);
    }


    /* pour une terminaison propre sur QUIT TERM INT SEGV... */
    signal(SIGQUIT, hdlr_fin);
    signal(SIGTERM, hdlr_fin);
    signal(SIGINT, hdlr_fin);
    signal(SIGSEGV, hdlr_fin);

    /* preparation de la structure d'adresse de la socket serveur sur 
       laquelle on va se connecter */
    memset(&a, 0, sizeof (a)); /* nettoyage de la structure */
    a.sun_family = AF_UNIX; /* famille de l'adresse */
    strncpy(a.sun_path, argv[1], sizeof (a.sun_path) - 1 /* 108 UNIX_PATH_MAX*/);

    /* creation de la socket client */
    if ((sfd = socket(PF_UNIX, SOCK_SEQPACKET, 0)) < 0) {
        perror("socket()");
        exit(EXIT_FAILURE);
    }

    /* Connexion au serveur */
    if (connect(sfd, (struct sockaddr *) &a, sizeof (a)) < 0) {
        perror("connect()");
        close(sfd);
        exit(EXIT_FAILURE);
    }
    /* Dans ce qui suit, n'oubliez pas de tester le code de retour des
     * fonctions utilisees et d'emettre un message d'erreur avec perror(),
     * puis de sortir avec exit(). */

    /* Boucle de communication */
    for (;;) {
        /* Lecture socket */
        nread = read(sfd, buf, BUFSIZE);
        if (nread == 0) {
            printf("Connexion rompue\n");
            exit(EXIT_SUCCESS);
        } else if (nread < 0) {
            perror("read");
            exit(EXIT_FAILURE);
        }
        buf[nread] = '\0';
        /* Affichage ecran du message lu sur la socket */
        printf("Message recu '%s'", buf);

        /* Lecture clavier. Si on tape <Control-D>, gets() rend NULL
         * <Control-D> symbolise la fin de fichier, ici la terminaison
         * du client */
        pt = fgets(buf, BUFSIZE, stdin);
        if (pt == NULL) {
            printf("Sortie du client\n");
            exit(EXIT_SUCCESS);
        }
        nwrite = write(sfd, buf, strlen(buf));
        if (nwrite < 0) {
            perror("write");
            exit(EXIT_FAILURE);
        }
    }
}
