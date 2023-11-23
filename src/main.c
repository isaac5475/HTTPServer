//
// Created by murat on 11/11/23.
//

#include "main.h"
#include "utils.h"

/*
** server.c -- a stream socket server demo
*/



#define BACKLOG 10   // how many pending connections queue will hold
const int SHM_SIZE = 1024;



void sigchld_handler(int s)
{
    // waitpid() might overwrite errno, so we save and restore it:
    int saved_errno = errno;

    while(waitpid(-1, NULL, WNOHANG) > 0);

    errno = saved_errno;
}


// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(int varc, char* argv[])
{
    if (varc != 3) {
        fprintf(stderr, "Usage: webserver [ipaddr] [port]");
        exit(1);
    }
    char* ipaddr = argv[1];
    char* port = argv[2];
    int sockfd, new_fd;  // listen on sock_fd, new connection on new_fd
    struct sockaddr_storage their_addr; // connector's address information
    struct sigaction sa;
    socklen_t sin_size;
    char s[INET6_ADDRSTRLEN];



    start_server(ipaddr, port, &sockfd);

    if (listen(sockfd, BACKLOG) == -1) {
        perror("listen");
        exit(1);
    }

    sa.sa_handler = sigchld_handler; // reap all dead processes
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }

    printf("server: waiting for connections...\n");

    while(1) {  // main accept() loop
        sin_size = sizeof their_addr;
        new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
        printf("accepted new connection: %d", new_fd);
        if (new_fd == -1) {
            perror("accept");
            continue;
        }

        inet_ntop(their_addr.ss_family,
                  get_in_addr((struct sockaddr *)&their_addr),
                  s, sizeof s);
        printf("server: got connection from %s\n", s);

//        if (!fork()) { // this is the child process
//            close(sockfd); // child doesn't need the listener
            char msgPrefix[REQUEST_LEN];
            memset(msgPrefix, 0, REQUEST_LEN);
            request_handler(new_fd, msgPrefix);
            close(new_fd);
//            printf("closing connection");
//            kill(getpid(), SIGTERM);
//        }
        close(new_fd);  // parent doesn't need this
    }

    return 0;
}

