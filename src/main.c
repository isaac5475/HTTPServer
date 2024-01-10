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
//    if (varc < 3) {
//        fprintf(stderr, "Usage: webserver [ipaddr] [port] [node_id]");
//        exit(1);
//    }
    char* ipaddr = argv[1];
    char* port = argv[2];
    char* res;
    int node_id = strtol(argv[3], &res, 10);
    int sockfd_tcp, new_fd_tcp;  // TCP: listen on sock_fd, new connection on new_fd
    int sockfd_udp;
    int num_bytes;
    struct sockaddr_storage their_addr_tcp; // connector's address information
    struct sockaddr_in their_addr_udp;
    struct sigaction sa;
    socklen_t sin_size;
    socklen_t addr_len;
    char s[INET6_ADDRSTRLEN];
    char buf[REQUEST_LEN];
    struct dynamicResource dynamicResources[MAX_RESOURCES_AMOUNT];
    struct data data;
    memset(dynamicResources, 0, MAX_RESOURCES_AMOUNT * sizeof(struct  dynamicResource*));
    sa.sa_handler = sigchld_handler; // reap all dead processes
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }

    struct dht dhtInstance;
    memset(&dhtInstance, 0, sizeof(struct dht));
    populate_dht_struct(&dhtInstance);
    dhtInstance.node_id = atol(argv[3]);
    data.dhtInstance = &dhtInstance;
    data.dhtInstance->node_ip = calloc(1, strlen(ipaddr)+1);
    memcpy(data.dhtInstance->node_ip, ipaddr, strlen(ipaddr));
    data.dhtInstance->node_port = atol(port);

    start_server_tcp(ipaddr, port, &sockfd_tcp);
    data.p = start_server_udp(ipaddr, port, &sockfd_udp);
    data.udpfd = sockfd_udp;
    data.oldest_record = 0;
    struct hash_record* hash_records[10];
    for (int i = 0; i < 10; i++) {
        hash_records[i] = calloc(1, sizeof(struct hash_record));
    }
    data.hash_records = (struct hash_record **) hash_records;
    data.dynamicResources = &dynamicResources;
    data.node_id = node_id;

    if (listen(sockfd_tcp, BACKLOG) == -1) {
        perror("listen");
        exit(1);
    }
        while (1) {  // main loop
            uint8_t udp_buff[11];
            memset(udp_buff, 0, 11);
            addr_len = sizeof their_addr_udp;
            ssize_t bytes_read = recvfrom(sockfd_udp, udp_buff, 11, 0,
                                (struct sockaddr *)&their_addr_udp, &addr_len);
            if (bytes_read == -1) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // Do nothing
                } else {
                    perror("recvfrom");
                    exit(1);
                }
            } else if (bytes_read > 0) {
//                printf("listener: got packet from %s\n",
//                       inet_ntop(their_addr_udp.ss_family,
//                                 get_in_addr((struct sockaddr *) &their_addr_udp),
//                                 s, sizeof s));
                printf("listener: packet is %d bytes long\n", bytes_read);
                udp_handler(udp_buff, data.udpfd, (struct sockaddr*)&their_addr_udp, addr_len, &data, data.dhtInstance);
            }

            //tcp handler

            sin_size = sizeof their_addr_tcp;
            new_fd_tcp = accept(sockfd_tcp, (struct sockaddr *) &their_addr_tcp, &sin_size);
            if (new_fd_tcp == -1) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    //Do Nothing and skip
                }
            } else {
                printf("accepted new connection: %d", new_fd_tcp);
                if (new_fd_tcp == -1) {
                    perror("accept");
                }

                inet_ntop(their_addr_tcp.ss_family,
                          get_in_addr((struct sockaddr *) &their_addr_tcp),
                          s, sizeof s);
                printf("server: got connection from %s\n", s);

                char msgPrefix[REQUEST_LEN];
                memset(msgPrefix, 0, REQUEST_LEN);
                request_handler(new_fd_tcp, msgPrefix, &data);
                close(new_fd_tcp);
            }
        }
}

