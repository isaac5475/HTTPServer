//
// Created by murat on 11/11/23.
//

#ifndef RN_PRAXIS_UTILS_H
#define RN_PRAXIS_UTILS_H


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include "sys/shm.h"
#include <time.h>
#include <openssl/sha.h>
#include "udp.h"


#define REQUEST_LEN 8192
#define MAX_HEADERS_AMOUNT 40
#define MAX_RESOURCES_AMOUNT 100
#define BACKLOG 10   // how many pending connections queue will hold
#define MAX_HOSTNAME_LEN 64

#define STATUS_CODE_400 "HTTP/1.1 400 Bad Request\r\n\r\n"
#define STATUS_CODE_400_CL "HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\n\r\n"
#define STATUS_CODE_404 "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n"
#define STATUS_CODE_404_CL "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n"
#define STATUS_CODE_501 "HTTP/1.1 501 Not Implemented\r\nContent-Length: 0\r\n\r\n"
#define STATUS_CODE_201 "HTTP/1.1 201 Created\r\nContent-Length: 0\r\n\r\n"
#define STATUS_CODE_204 "HTTP/1.1 204 No Content\r\nContent-Length: 0\r\n\r\n"
#define STATUS_CODE_403 "HTTP/1.1 403 Forbidden\r\nContent-Length: 0\r\n\r\n"
#define STATUS_CODE_503 "HTTP/1.1 503 Service Unavailable\r\nRetry-After: 1\r\nContent-Length: 0\r\n\r\n"
#define DYNAMIC_ROUTE "/dynamic/"


struct dynamicResource {
    char* key;
    char* value;
};

struct header {
    char* key;
    char* value;
};

struct httpRequest {
    char* HTTPMethode;
    char* route;
    int routeLen;
    char* HTTPVersion;
    struct header* headers[MAX_HEADERS_AMOUNT];
    char* payload;
    int status;
};

struct data {
    struct dynamicResource **dynamicResources;
    int node_id;
    struct dht* dhtInstance;
    struct addrinfo* p;
    int udpfd;
    char* ipaddr;
    uint16_t port;
    struct hash_record** hash_records;
    uint8_t oldest_record;
    fd_set *fdset;
};
struct dht {
    uint16_t node_id;
    char* node_ip;
    uint16_t node_port;
    uint16_t prev_node_id;
    uint16_t succ_id;
    char* prev_ip;
    int prev_port;
    char* succ_ip;
    int succ_port;
};
struct hash_record {
    uint16_t node_id;
    uint16_t hash_id;
    char host[MAX_HOSTNAME_LEN];
};
int start_server_tcp(char* ipaddr, char* port, int* sockfd);
struct addrinfo* start_server_udp(char* ipaddr, char* port, int* sockfd);
void request_handler(int fd, char* msgPrefix, struct data* data);
int parse_requests(char* msg, struct httpRequest* reqs[10], int* position);
int parse_request(char* msg, struct httpRequest* req);
int parse_headers(char* msg, struct header* headers[]);
void get_handler(int fd, struct httpRequest *req, struct dynamicResource* dynamicResources[MAX_RESOURCES_AMOUNT]);
void put_handler(int fd, struct httpRequest *req, struct dynamicResource* dynamicResources[MAX_RESOURCES_AMOUNT]);
void delete_handler(int fd, struct httpRequest *req, struct dynamicResource* dynamicResources[MAX_RESOURCES_AMOUNT]);
char* read_dynamic_record(char* requestedRoute, struct dynamicResource* dynamicResources[MAX_RESOURCES_AMOUNT]);
int add_dynamic_record(char* requestedRoute, char* payload, struct dynamicResource* dynamicResources[MAX_RESOURCES_AMOUNT]);
int delete_dynamic_record(char* requestedRoute, struct dynamicResource* dynamicResources[MAX_RESOURCES_AMOUNT]);
void free_httpRequest(struct httpRequest* req);
char* append_strings(char* str1, char* str2);
void free_dynamic_records(struct dynamicResource* dynamicResources[MAX_RESOURCES_AMOUNT]);
uint16_t hash(const char* str);
int get_prev_node_id();
int populate_dht_struct(struct dht* dht);
void populate_hash_records(struct data* data);
uint8_t node_is_responsible(const struct data *data, uint16_t id);
#endif //RN_PRAXIS_UTILS_H