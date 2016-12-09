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

#define BUFSIZE 512
char * sockname; /* nom de la socket */
int  sfd;

void communication(int, struct sockaddr *, socklen_t);

void fin_fils(int n) {
    int status;
    int fils = wait(&status);
    printf("Fils numero: %d\n", fils);

    if (WIFEXITED(status))
        printf("termine sur exit(%d)\n", WEXITSTATUS(status));

    if (WIFSIGNALED(status))
        printf("termine sur signal %d\n", WTERMSIG(status));

    //exit(EXIT_SUCCESS); /* pour terminer le pere */
}

void hdlr_fin(int sig) {
    printf("Reception du signal %d. Arret du bus !\n", sig);
    close(sfd);
    unlink(sockname);
    exit(EXIT_SUCCESS);
}

int main(int argc, char **argv) {
    int s, ns, r;
    struct sockaddr_un a;
    struct addrinfo *result, *rp;
    struct sockaddr_storage from;
    socklen_t fromlen;
    //char *message = "Message a envoyer: ";

    if (argc != 2) {
        printf("Usage: %s  socket\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    /* pour une terminaison propre sur QUIT TERM INT SEGV... */
    signal(SIGQUIT, hdlr_fin);
    signal(SIGTERM, hdlr_fin);
    signal(SIGINT, hdlr_fin);
    signal(SIGSEGV, hdlr_fin);
    
    /* Construction de l'adresse locale (pour bind) */
    memset(&a, 0, sizeof (a));
    a.sun_family = AF_UNIX;
    sockname = strncpy(a.sun_path, (char *) argv[1], sizeof (a.sun_path) - 1);

    /* creation de la socket serveur */
    if ((sfd = socket(PF_UNIX, SOCK_SEQPACKET, 0)) < 0) {
        perror("socket()");
        exit(EXIT_FAILURE);
    }
    if (rp = bind(sfd, (struct sockaddr *) &a, sizeof(struct sockaddr_un) ) < 0) {
        perror("bind()");
        close(sfd);
        exit(EXIT_FAILURE);
    }
    printf("Socket bind\n");

    /* Positionnement de la machine a etats TCP sur listen */
    if (listen(sfd, 100) == 0) {
        printf("listen ok!\n");
    } else {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    for (;;) {
        printf("Start of listening loop\n");
        /* Acceptation de connexions */
        fromlen = sizeof (from);
        memset(&result, 0, sizeof (struct addrinfo));
        ns = accept(sfd, (struct sockaddr *) &from, &fromlen);
        if (ns == -1) {
            perror("accept");
            exit(EXIT_FAILURE);
        }
        //communication(ns, (struct sockaddr *) &from, fromlen);
        //*
        int pid = fork();
        switch (pid) {
            case -1:
                fprintf(stdout, "Erreur à la creation du fils");
                break;
            case 0:
                //Code fils
                communication(ns, (struct sockaddr *) &from, fromlen);
                exit(0);
            default:
                //Code père
                printf("I am your father!\n");
                signal(SIGCHLD, fin_fils);
        }
        //*/
    }
}

void communication(int ns, struct sockaddr *from, socklen_t fromlen) {
    char host[NI_MAXHOST];
    ssize_t nread, nwrite;
    char buf[BUFSIZE];
    /* Reconnaissance de la machine cliente */
    int s = getnameinfo((struct sockaddr *) from, fromlen,
            host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
    if (s == 0)
        printf("Debut avec client '%s'\n", host);
    else
        fprintf(stderr, "getnameinfo: %s\n", gai_strerror(s));

    for (;;) {
        printf("Start interact loop\n");
        nwrite = write(ns, "Message a envoyer: ", strlen("Message a envoyer: "));
        if (nwrite < 0) {
            perror("write");
            close(ns);
            break;
        }
        printf("Message send\n");
        nread = read(ns, buf, BUFSIZE);
        if (nread == 0) {
            printf("Fin avec client '%s'\n", host);
            close(ns);
            break;
        } else if (nread < 0) {
            perror("read");
            close(ns);
            break;
        }
        buf[nread] = '\0';
        printf("Message recu '%s'\n", buf);
    }
}
