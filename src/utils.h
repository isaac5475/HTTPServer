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

#define REQUEST_LEN 8192
#define MAX_HEADERS_AMOUNT 40
#define MAX_RESOURCES_AMOUNT 100
#define BACKLOG 10   // how many pending connections queue will hold
#define STATUS_CODE_400 "HTTP/1.1 400 Bad Request\r\n\r\n"
#define STATUS_CODE_400_CL "HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\n\r\n"
#define STATUS_CODE_404 "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n"
#define STATUS_CODE_404_CL "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n"
#define STATUS_CODE_501 "HTTP/1.1 501 Not Implemented\r\nContent-Length: 0\r\n\r\n"
#define STATUS_CODE_201 "HTTP/1.1 201 Created\r\nContent-Length: 0\r\n\r\n"
#define STATUS_CODE_204 "HTTP/1.1 204 No Content\r\nContent-Length: 0\r\n\r\n"
#define STATUS_CODE_403 "HTTP/1.1 403 Forbidden\r\nContent-Length: 0\r\n\r\n"
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


int start_server(char* ipaddr, char* port, int* sockfd);
void request_handler(int fd, char* msgPrefix, struct dynamicResource* dynamicResources[MAX_RESOURCES_AMOUNT]);
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
#endif //RN_PRAXIS_UTILS_H