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
    int node_id = 0;
    char* res;
    if (varc > 3) {
         node_id = strtol(argv[3], &res, 10);
         if (*res != '\0') {
             node_id = 0;
         }
    }
    int new_fd_tcp;  // TCP: listen on sock_fd, new connection on new_fd
    struct sockaddr_storage their_addr_tcp; // connector's address information
    struct sockaddr_in their_addr_udp;
    struct sigaction sa;
    socklen_t sin_size;
    socklen_t addr_len;
    char s[INET6_ADDRSTRLEN];
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
    data.dhtInstance = &dhtInstance;
    populate_dht_struct(&data, node_id, ipaddr, atol(port));

    start_server_tcp(ipaddr, port, &data.sockfd_tcp);
    data.p = start_server_udp(ipaddr, port, &data.sockfd_udp);
    data.oldest_record = 0;
    struct hash_record* hash_records[10];
    for (int i = 0; i < 10; i++) {
        hash_records[i] = calloc(1, sizeof(struct hash_record));
    }
    data.hash_records = (struct hash_record **) hash_records;
    populate_hash_records(&data);
    data.dynamicResources = &dynamicResources;

    fd_set master;
    fd_set read_fds;
    int fdmax;
    data.fdset = &master;
    FD_ZERO(&master);
    FD_ZERO(&read_fds);

    if (listen(data.sockfd_tcp, BACKLOG) == -1) {
        perror("listen");
        exit(1);
    }
    FD_SET(data.sockfd_tcp, &master);
    fdmax = data.sockfd_tcp;
    FD_SET(data.sockfd_udp, &master);
    if (data.sockfd_udp > fdmax) {
        fdmax = data.sockfd_udp;
    }
    char msgPrefix[REQUEST_LEN];
    memset(msgPrefix, 0, REQUEST_LEN);
    if (varc == 6) {
        data.started_with_anchor = 1;
    } else {
        data.started_with_anchor = 0;
    }

    if (data.started_with_anchor == 1) {
        send_join_msg_on_start(&data, argv[4], atol(argv[5]));
    }

    while (1) {  // main loop
        read_fds = master;
        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 100000;
        if (select(fdmax+1, &read_fds, NULL, NULL, &tv) == -1) {
            perror("select");
            exit(4);
        }
        if (data.dhtInstance->succ_id != data.dhtInstance->node_id) {
            send_stabilize_msg(data.sockfd_udp, &data);
        }
        for (int i = 0; i <= fdmax; i++) {
            if (FD_ISSET(i, &read_fds)) {
                if (i == sockfd_udp) {
                    //handle udp
                    uint8_t udp_buff[11];
                    memset(udp_buff, 0, 11);
                    addr_len = sizeof their_addr_udp;
                    ssize_t bytes_read = recvfrom(i, udp_buff, 11, 0,
                                                  (struct sockaddr *)&their_addr_udp, &addr_len);
                    if (bytes_read == -1) {
                        perror("recvfrom");
                        exit(1);
                    } else if (bytes_read > 0) {
                        udp_handler(udp_buff, data.sockfd_udp, (struct sockaddr*)&their_addr_udp, addr_len, &data, data.dhtInstance);
                    }
                }
                else if (i == data.sockfd_tcp) {
                    //handle tcp
                    sin_size = sizeof their_addr_tcp;
                    new_fd_tcp = accept(data.sockfd_tcp, (struct sockaddr *) &their_addr_tcp, &sin_size);
                    if (new_fd_tcp == -1) {
                        perror("accept");
                    } else {
                        FD_SET(new_fd_tcp, &master);
                        if (new_fd_tcp > fdmax) {
                            fdmax = new_fd_tcp;
                        }
                        printf("select server: accepted new connection: %d\n", new_fd_tcp);
                        if (new_fd_tcp == -1) {
                            perror("accept");
                        }

                        inet_ntop(their_addr_tcp.ss_family,
                                  get_in_addr((struct sockaddr *) &their_addr_tcp),
                                  s, sizeof s);
                        printf("server: got connection from %s\n", s);

                    }
                }
                else {
                    // handle tcp connection
                    printf("handling connection with TCP socket %d\n", i);
                    request_handler(i, msgPrefix, &data);
                }
            }
        }
    }
}

void send_join_msg_on_start(struct data *data, const char *anchor_ip, uint16_t anchor_port) {
    uint8_t packet[11];
    struct in_addr node__addr;
    if (inet_pton(AF_INET, (*data).dhtInstance->node_ip, &node__addr) <= 0) {
        perror("inet_pton");
    }
    create_msg(packet, JOIN_MODE, 0, (*data).dhtInstance->node_id, &node__addr, (*data).dhtInstance->node_port);
    struct sockaddr_in anchor_addr;
    memset(&anchor_addr, 0, sizeof(anchor_addr));

    anchor_addr.sin_family = AF_INET;
    anchor_addr.sin_port = htons(anchor_port);
    if (inet_pton(AF_INET, anchor_ip, &anchor_addr.sin_addr) <= 0) {
        perror("inet_pton");
    }
    send_msg((*data).sockfd_udp, packet, &anchor_addr);
}

