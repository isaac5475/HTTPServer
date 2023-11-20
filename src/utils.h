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

#define REQUEST_LEN 256
#define MAX_HEADERS_AMOUNT 10
#define STATUS_CODE_400 "HTTP/1.1 400 Bad Request\r\n\r\n"
#define STATUS_CODE_404 "HTTP/1.1 404 Not Found\r\n\r\n"
#define STATUS_CODE_501 "HTTP/1.1 501 Not Implemented\r\n\r\n"
#define STATUS_CODE_200 "HTTP/1.1 200 OK\r\n\r\n"
#define STATUS_CODE_201 "HTTP/1.1 201 Created\r\n\r\n"
#define STATUS_CODE_204 "HTTP/1.1 204 No Content\r\n\r\n"
#define STATUS_CODE_403 "HTTP/1.1 403 Forbidden\r\n\r\n"
#define STATUS_REPLY "Reply\r\n\r\n"
#define MAX_HEADERS_AMOUNT 10

#define HTTP_VERSION "HTTP/1.1"

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
void request_handler(int fd);
int parse_requests(char* msg, struct httpRequest* reqs[10]);
int parse_request(char* msg, struct httpRequest* req);
int parse_headers(char* msg, struct header* headers[]);
void get_handler(int fd, struct httpRequest *req);
void put_handler(int fd, struct httpRequest *req);
void delete_handler(int fd, struct httpRequest *req);
char* read_dynamic_record(char* requestedRoute);
int add_dynamic_record(char* requestedRoute, char* payload);
int delete_dynamic_record(char* requestedRoute);
void free_httpRequest(struct httpRequest* req);
#endif //RN_PRAXIS_UTILS_H