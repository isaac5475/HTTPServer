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
#define STATUS_CODE_201 "HTTP/1.1 201 Created\r\n\r\n"
#define STATUS_CODE_204 "HTTP/1.1 204 No Content\r\n\r\n"
#define STATUS_CODE_403 "HTTP/1.1 403 Forbidden\r\n\r\n"

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
    struct header* headers[10];
    char* payload;
    int status;
};

struct llRoot {
    struct entryNode* start;
};

struct entryNode {
    char key[16];
    char val[16];
};


int start_server(char* ipaddr, char* port, int* sockfd);
void request_handler(int fd, int shmid);
int parse_requests(char* msg, struct httpRequest* reqs[10]);
int parse_request(char* msg, struct httpRequest* req);
int parse_headers(char* msg, struct header* headers[]);
void get_handler(int fd, struct httpRequest *req, int shmid);
void put_handler(int fd, struct httpRequest *req, int shmid);
void delete_handler(int fd, struct httpRequest *req, int shmid);
struct entryNode* findNodeByKey(int shmid, char* key, size_t len);
int saveInLL(int shmid, char* key, size_t key_len, char* val, size_t val_len);
int removeFromLLByKey(int shmid, char* key, size_t keyLen);
#endif //RN_PRAXIS_UTILS_H