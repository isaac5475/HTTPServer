//
// Created by murat on 11/11/23.
//

#include "main.h"
#include "utils.h"




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
    int sockfd_tcp, new_fd_tcp;  // TCP: listen on sock_fd, new connection on new_fd
    int sockfd_udp, new_fd_udp;  // UDP: listen on sock_fd, new connection on new_fd
    int num_bytes;
    struct sockaddr_storage their_addr; // connector's address information
    struct sigaction sa;
    socklen_t sin_size;
    socklen_t addr_len;
    char s[INET6_ADDRSTRLEN];
    char buf[REQUEST_LEN];
    struct dynamicResource* dynamicResources[MAX_RESOURCES_AMOUNT];
    memset(dynamicResources, 0, MAX_RESOURCES_AMOUNT * sizeof(struct  dynamicResource*));



    start_server_tcp(ipaddr, port, &sockfd_tcp);
    start_server_udp(ipaddr, port, &sockfd_udp);

    if (listen(sockfd_tcp, BACKLOG) == -1) {
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

    printf("Listener: waiting to recvfrom...\n");

    while(1) {  // main recvfrom() loop
        addr_len = sizeof their_addr;

        if ((num_bytes = recvfrom(sockfd_udp, buf, REQUEST_LEN - 1, 0,
                                  (struct sockaddr *) &their_addr, &addr_len)) == -1) {
            perror("recvfrom");
            exit(1);
        }

        printf("listener: got packet from %s\n",
               inet_ntop(their_addr.ss_family,
                         get_in_addr((struct sockaddr *) &their_addr),
                         s, sizeof s));
        printf("listener: packet is %d bytes long\n", num_bytes);
        buf[num_bytes] = '\0';
        printf("listener: packet contains \"%s\"\n", buf);
        close(sockfd_udp);
//        sin_size = sizeof their_addr;
//        new_fd_tcp = accept(sockfd_tcp, (struct sockaddr *)&their_addr, &sin_size);
//        printf("accepted new connection: %d", new_fd_tcp);
//        if (new_fd_tcp == -1) {
//            perror("accept");
//            continue;
//        }
//
//        inet_ntop(their_addr.ss_family,
//                  get_in_addr((struct sockaddr *)&their_addr),
//                  s, sizeof s);
//        printf("server: got connection from %s\n", s);
//
//            char msgPrefix[REQUEST_LEN];
//            memset(msgPrefix, 0, REQUEST_LEN);
//
//        request_handler(new_fd_tcp, msgPrefix, dynamicResources);
//            close(new_fd_tcp);
//        close(new_fd_tcp);  // parent doesn't need this
//    }
//    free_dynamic_records(dynamicResources);
//    return 0;
    }
}

