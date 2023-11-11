//
// Created by murat on 11/11/23.
//

#ifndef RN_PRAXIS_UTILS_H
#define RN_PRAXIS_UTILS_H


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>

#define REQUEST_LEN 256
#define MAX_HEADERS_AMOUNT 10
#define STATUS_CODE_400 "400 Bad Request"
#define STATUS_CODE_404 "404 Not Found"
#define STATUS_CODE_501 "501 Not Implemented"
struct header {
    char* key;
    char* value;
};

struct httpRequest {
    char* HTTPMethode;
    char* route;
    char* HTTPVersion;
    struct header* headers[10];
    char* payload;
    int status;
} ;

int start_server(char* ipaddr, char* port, int* sockfd);
void request_handler(int fd);
int parse_request(char* msg, struct httpRequest* req);
int parse_headers(char* msg, struct header* headers[]);
void get_handler(int fd, struct httpRequest *req);
#endif //RN_PRAXIS_UTILS_H