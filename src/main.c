//
// Created by murat on 11/11/23.
//

#include "main.h"
#include "utils.h"




const int SHM_SIZE = 1024;


pid_t child_pid;
void sigterm_handler(int signum) {
    // Handle termination signal
    printf("Child process received termination signal. Exiting.\n");
    exit(EXIT_SUCCESS);
}
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
    int node_id = 0;
    if (varc > 3) {
         node_id = strtol(argv[3], &res, 10);
         if (*res != '\0') {
             node_id = 0;
         }
    }
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
    dhtInstance.node_id = node_id;
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
    populate_hash_records(&data);
    data.dynamicResources = &dynamicResources;
    data.node_id = node_id;

    FILE* file = fopen("log.txt", "w");

    fd_set master;
    fd_set read_fds;
    int fdmax;
    data.fdset = &master;
    FD_ZERO(&master);
    FD_ZERO(&read_fds);

    if (listen(sockfd_tcp, BACKLOG) == -1) {
        perror("listen");
        exit(1);
    }
    FD_SET(sockfd_tcp, &master);
    fdmax = sockfd_tcp;
    FD_SET(sockfd_udp, &master);
    if (sockfd_udp > fdmax) {
        fdmax = sockfd_udp;
    }
    char msgPrefix[REQUEST_LEN];
    memset(msgPrefix, 0, REQUEST_LEN);

    if (varc == 6) {
        char* anchor_ip = argv[4];
        uint16_t anchor_port = atol(argv[5]);

        uint8_t packet[11];

        struct in_addr node__addr;
        memset(data.dhtInstance->node_ip, 0, sizeof(*data.dhtInstance->node_ip));
        if (inet_pton(AF_INET, data.dhtInstance->node_ip, &node__addr) <= 0) {
            perror("inet_pton");
        }
        create_msg(packet, JOIN_MODE, 0, data.dhtInstance->node_id, &node__addr, data.dhtInstance->node_port);
        struct sockaddr_in anchor_addr;
        memset(&anchor_addr, 0, sizeof(anchor_addr));

        anchor_addr.sin_family = AF_INET;
        anchor_addr.sin_port = htons(anchor_port);
        if (inet_pton(AF_INET, anchor_ip, &anchor_addr.sin_addr) <= 0) {
            perror("inet_pton");
        }
        send_msg(data.udpfd, packet, &anchor_addr);
        printf("send packet to %s:%d\n", anchor_ip, anchor_port);
    }

//    setpgid(0, 0);
     child_pid = fork(); //child process sends stabilize messages every 1s
    if (child_pid == -1) {
        perror("fork");
    } else if (child_pid == 0) {
        signal(SIGTERM, sigterm_handler);  // Install signal handler
        printf("child process with PID: %d\n", getpid());
       while(1) {
           sleep(1);
           send_stabilize_msg(data.udpfd, &data);
       }
    } else { //parent process
        printf("parent process with PID: %d\n", getpid());
        while (1) {  // main loop
            read_fds = master;
            if (select(fdmax+1, &read_fds, NULL, NULL, NULL) == -1) {
                perror("select");
                exit(4);
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
                            printf( "listener: packet is %d bytes long on UDP socket %d\n", bytes_read, i);
                            udp_handler(udp_buff, data.udpfd, (struct sockaddr*)&their_addr_udp, addr_len, &data, data.dhtInstance);
                        }
                    }
                    else if (i == sockfd_tcp) {
                        //handle tcp
                        sin_size = sizeof their_addr_tcp;
                        new_fd_tcp = accept(sockfd_tcp, (struct sockaddr *) &their_addr_tcp, &sin_size);
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
}

