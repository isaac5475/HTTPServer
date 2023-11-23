//
// Created by murat on 11/11/23.
//

#include "utils.h"

int start_server(char* ipaddr, char* port, int* sockfd) {
    struct addrinfo hints, *servinfo, *p;
    int yes = 1;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // use my IP
    int rv;

    if ((rv = getaddrinfo(ipaddr, port, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and bind to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((*sockfd = socket(p->ai_family, p->ai_socktype,
                             p->ai_protocol)) == -1) {
            perror("server: socket");
            continue;
        }

        if (setsockopt(*sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
                       sizeof(int)) == -1) {
            perror("setsockopt");
            exit(1);
        }

        if (bind(*sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(*sockfd);
            perror("server: bind");
            continue;
        }

        break;
    }

    freeaddrinfo(servinfo); // all done with this structure

    if (p == NULL)  {
        fprintf(stderr, "server: failed to bind\n");
        exit(1);
    }
}

void request_handler(int fd, char* msgPrefix) {
    while (1) { //Connection remains open until closed client-side
        char msgBuf[REQUEST_LEN];
        memset(msgBuf, 0, REQUEST_LEN);
        int recvRes = recv(fd, msgBuf, REQUEST_LEN, 0);
        if (recvRes == -1) {
            continue;
        }
        if (recvRes == 0) {
            printf("connection reset\r\n");
            return;
        }
        char* newMsg = append_strings(msgPrefix, msgBuf);
        memset(msgPrefix, 0, REQUEST_LEN);
        printf("msgPrefix: %sstrlen(msgPrefix): %d\r\n", msgPrefix, strlen(msgPrefix));
        printf("newMsg: %s\r\nstrlen(newMsg): %d\r\n", newMsg, strlen(newMsg));
        int position = 0;
        struct httpRequest** requests = calloc(10, sizeof(struct httpRequest*));
        int parse_res = parse_requests(newMsg, requests, &position);
        if (position != strlen(newMsg)) { //if we couldn't parse the end of the incoming message, save it till next time
            strncpy(msgPrefix, newMsg + position, strlen(newMsg - position));
        }
        free(newMsg);
//        printf("-----------------------\r\n");
        if (parse_res == 0) {
            continue;
        }
        for (int i = 0; i < parse_res; i++) {

            if (requests[i]->status == -1) {
                send(fd, STATUS_CODE_400, strlen(STATUS_CODE_400), 0);
                free_httpRequest((requests[i]));
                continue;
            }

            if (strncmp(requests[i]->HTTPMethode, "GET", strlen("GET")) == 0) {
                get_handler(fd, requests[i]);
                free_httpRequest((requests[i]));
                continue;
            }
            if (strncmp(requests[i]->HTTPMethode, "PUT", strlen("PUT")) == 0) {
                put_handler(fd, requests[i]);
                free_httpRequest((requests[i]));
                continue;
            }
            if (strncmp(requests[i]->HTTPMethode, "DELETE", strlen("DELETE")) == 0) {
                delete_handler(fd, requests[i]);
                free_httpRequest((requests[i]));
                continue;
            }
            send(fd, STATUS_CODE_501, strlen(STATUS_CODE_501), 0);
            free_httpRequest(requests[i]);
        }
        free(requests);
//        return;
    }
}

void put_handler(int fd, struct httpRequest *req) {
//    printf("starting sending...\r\n");
//    char* resp = "HTTP/1.1 201 Created\r\n\r\n";
//    int b =send(fd,resp, strlen(resp), 0);
//    send(fd, STATUS_CODE_201, strlen(STATUS_CODE_201) + 1, 0);
//    shutdown(fd, SHUT_WR);
//    return;
    int content_len_val = -1;
    const char clkey[] = "Content-Length";
    for (int i = 0; i < MAX_HEADERS_AMOUNT && req->headers[i] != NULL; i++) {
        if (strncmp(req->headers[i]->key, clkey, strlen(clkey)) == 0) {
            content_len_val = strtol(req->headers[i]->value, NULL, 10);
            if (content_len_val == 0) {//Couldn't parse or header == 0, which is invalid behaviour
                send(fd, STATUS_CODE_400, strlen(STATUS_CODE_400), 0);
                return;
            } else break;
        }
    }
    if (content_len_val == -1 || req->payload == NULL ) { //wasn't found
        send(fd, STATUS_CODE_400, strlen(STATUS_CODE_400), 0);
        shutdown(fd, SHUT_WR);
        return;
    }
    int pos = 0;
    char dynamicRoute[] = "/dynamic/";
    if (strncmp(req->route, dynamicRoute, strlen(dynamicRoute)) == 0) {
        pos += strlen(dynamicRoute);
        int res = add_dynamic_record(req->route + strlen("/dynamic/"), req->payload);

        printf("dynamic put...\r\n");
        if (res == 1) {
            printf("starting sending 201...\r\n");
//            char* resp = "HTTP/1.1 201 Created\r\n\r\n";
            send(fd,STATUS_CODE_204, strlen(STATUS_CODE_204), 0);
            shutdown(fd, SHUT_WR);
        } else if (res == 0) {    //overriden
            char *resp = STATUS_CODE_204;
            int d = send(fd, resp, strlen(resp), 0);
            shutdown(fd, SHUT_WR);
        } else {
          send(fd, STATUS_CODE_403, strlen(STATUS_CODE_403), 0);
        }
    } else {
        send(fd, STATUS_CODE_403, strlen(STATUS_CODE_403), 0);
        shutdown(fd, 2);
    }
}

void delete_handler(int fd, struct httpRequest *req) {
    const char dynamicRoute[] = "/dynamic/";
    if (strncmp(req->route, dynamicRoute, strlen(dynamicRoute)) == 0) {
        int res = delete_dynamic_record(req->route + strlen("/dynamic/"));
        if (res == -1) {
            send(fd, STATUS_CODE_404, strlen(STATUS_CODE_404), 0);
            shutdown(fd, SHUT_WR);
            return;
        }
        send(fd, STATUS_CODE_204, strlen(STATUS_CODE_204), 0);
    } else {
        send(fd, STATUS_CODE_403, strlen(STATUS_CODE_403), 0);
    }
}



void get_handler(int fd, struct httpRequest *req) {
    printf("handling get\r\n");

    const char foo_url[] = "/static/foo";
    const char bar_url[] = "/static/bar";
    const char baz_url[] = "/static/baz";
    const char dynamic_url[] = "/dynamic/";
    if (strncmp(req->route, foo_url, strlen(foo_url)) == 0) {
        printf("starting sending...\r\n");
        char* resp = "HTTP/1.1 200 OK\r\nContent-Length: 3\r\n\r\nFoo";
        int b = send(fd, resp, strlen(resp), 0);
        printf("resp sent, bytes sent: %d", b);
    } else if (strncmp(req->route, bar_url, strlen(bar_url)) == 0) {
        char* resp = "HTTP/1.1 200 OK\r\nContent-Length: 3\r\n\r\nBar";
        send(fd, resp, strlen(resp), 0);
    } else if (strncmp(req->route, baz_url, strlen(baz_url)) == 0) {
        char *resp = "HTTP/1.1 200 OK\r\nContent-Length: 3\r\n\r\nBaz";
        send(fd, resp, strlen(resp), 0);
    } else if (strncmp(req->route, dynamic_url, strlen(dynamic_url)) == 0) {
        printf("handling dynamic\r\n");
        char* payload = read_dynamic_record(req->route + sizeof("/dynamic"));
      char resp[REQUEST_LEN];
      if (payload == NULL) {
          printf("starting sending...\r\n");
          char* resp = "HTTP/1.1 404 Not Found\r\n\r\n";
//          char* resp = "HTTP/1.1 200 OK\r\nContent-Length: 3\r\n\r\nFoo";
          int b = send(fd,resp, strlen(resp), 0);
          shutdown(fd, SHUT_WR);
          printf("404 sent, bytes sent: %d", b);
          return;
      }
        printf("past 404\r\n");
        sprintf(resp, "HTTP/1.1 200 OK\r\nContent-Length: %d\r\n\r\n%s", (int)strlen(payload), payload);
        send(fd, resp, strlen(resp), 0);
        free(payload);
    } else {
        send(fd, STATUS_CODE_404, strlen(STATUS_CODE_404), 0);
    }
}

char* read_dynamic_record(char* requestedRoute) {
    char dir[MAX_DIR_NAME];
    memset(dir, 0, MAX_DIR_NAME);
    snprintf(dir, sizeof(dir), "dynamic/%s.txt", requestedRoute);
    if (access(dir, F_OK) == -1) { //resource doesn't exist
        return NULL;
    }
    FILE *file = fopen(dir, "r");
    if (file == NULL) {
        return NULL;
    }
    int size;
    fseek(file, 0, SEEK_END);

    // Get the position, which is the size of the file
    size = ftell(file);

    // Restore the position
    fseek(file, 0, SEEK_SET);
    char* payload = calloc(1, size + 1);
    fgets(payload, size+1, file);
    fclose(file);
    return payload;
}

int add_dynamic_record(char* requestedRoute, char* payload) {
    char dir[MAX_DIR_NAME];
    memset(dir, 0, MAX_DIR_NAME);
    snprintf(dir, sizeof(dir), "dynamic/%s.txt", requestedRoute);
    if (access(dir, F_OK) == -1) { //resource doesn't exist
        FILE *file = fopen(dir, "w");
        if (file == NULL) return -1;
        fprintf(file, "%s", payload);
        fclose(file);
        return 1;
    } else {    //file already exists, rewrite
        FILE *file = fopen(dir, "w");
        if (file == NULL) {
            return -1;
        }
        fprintf(file, "%s", payload);
        fclose(file);
        return 0;
    }
}

int delete_dynamic_record(char * requestedRoute) {
    char dir[32];
    sprintf(dir, "dynamic/%s.txt", requestedRoute);
    return remove(dir);
}

int parse_requests(char* msg, struct httpRequest* reqs[10], int* position) { //return value - -1 if error, amount of parsed reqs otherw
    int reqsi = 0;
    int pos = 0;
    while (reqsi < 10) {
        struct httpRequest* parsedReq = calloc(1, sizeof(struct httpRequest));
        int res = parse_request(msg + pos, parsedReq);
        if (res == -1) {
            if (parsedReq->status > -2) { //if the req is complete
                reqs[reqsi++] = parsedReq;
            }
            break;
        }
        if (res == 0) {
            break;
        }
        pos += res;
        reqs[reqsi++] = parsedReq;
        *position += res;
    }
    return reqsi;
}

int parse_request(char* msg, struct httpRequest* req) { //req->status = -1 means the request is invalid, -2 not complete
    req->status = 0;
    int pos = 0;
    if (strlen(msg) == 0) return 0;
    if (strncmp(msg, "GET", strlen("GET")) == 0) { //   parse method
        pos += strlen("GET");
        req->HTTPMethode = (char *) calloc(1, sizeof("GET"));
        strncpy(req->HTTPMethode, msg, strlen("GET"));
    } else if (strncmp(msg, "PUT", strlen("PUT")) == 0) {
        pos += strlen("PUT");
        req->HTTPMethode = (char *) calloc(1, sizeof("PUT"));
        strncpy(req->HTTPMethode, msg, strlen("PUT"));
    } else if (strncmp(msg, "DELETE", strlen("DELETE")) == 0) {
        pos += strlen("DELETE");
        req->HTTPMethode = (char *) calloc(1, sizeof("DELETE"));
        strncpy(req->HTTPMethode, msg, strlen("DELETE"));
    } else if (strchr(msg, ' ') != NULL) {
        int methLen = strchr(msg, ' ') - msg;
        pos += methLen;
        req->HTTPMethode = (char *) calloc(1, methLen);
        strncpy(req->HTTPMethode, msg, methLen);
    } else {
        req->status = -1;
        return -1;
    }

    pos += 1; // ' '
    long long lenOfUrl = strchr(msg + pos, ' ') - (msg + pos);  //  parse URL
    req->routeLen = lenOfUrl;
    req->route = (char *) calloc(1, lenOfUrl + 1);
    strncpy(req->route, msg + pos, lenOfUrl);
    pos += lenOfUrl;

    pos += 1; // ' '

    const char httpVersion[] = "HTTP/1.1";
    if (strncmp(msg + pos, httpVersion, strlen(httpVersion)) == 0) {
        req->HTTPVersion = (char *) calloc(1, sizeof(httpVersion));
        strncpy(req->HTTPVersion, msg + pos, strlen(httpVersion));
        pos += strlen(httpVersion);
    } else {
        req->status = -1;
        return -1;
    }
    char crlf[] = "\r\n";
    if (strncmp(msg + pos, crlf, strlen(crlf)) != 0) {
        req->status = -1;
        return -1;
    }
    pos += strlen("\r\n");
    int headersLen = parse_headers(msg + pos, req->headers);
    if ( headersLen == -1) { //didn't find end of headers -> incomplete
        req->status = -2;
        return -1;
    }
    pos += headersLen;
    req->payload = NULL;
    if (strncmp(req->HTTPMethode, "PUT", 3) != 0) {
        return pos;
    }

    int contentLen = -1;
    for (int i = 0; i < 10 && req->headers[i] != NULL; i++) {
        if (strncmp(req->headers[i]->key, "Content-Length", strlen("Content-Length")) == 0) {
            contentLen = atoi(req->headers[i]->value);
            break;
        }
    }
    if (contentLen == -1) {
        req->status = -1;
        return pos;
    }
    req->payload = calloc(contentLen + 1, sizeof(char));
    strncpy(req->payload, msg+pos, contentLen);
    return pos + contentLen;
//    if (strncmp(msg + pos, crlf, strlen(crlf)) == 0) {    //no payload;
//        req->payload = NULL;
//        return pos + strlen(crlf);
//    }
//    char* endOfPayload = strstr(msg + pos, crlf);
//    if (endOfPayload == NULL) {
//        return -1;
//    }
//    size_t lenOfPayload = endOfPayload - (msg + pos);
//    req->payload = malloc(lenOfPayload);
//    strncpy(req->payload, msg + pos, lenOfPayload);

//    return pos + payloadLen + strlen(crlf);


}

int parse_headers(char* msg, struct header** headers) {
    int pos = 0;
    for (int i = 0; i < MAX_HEADERS_AMOUNT; i++) {
       headers[i] = NULL;
    }
    char crlf[] = "\r\n";
    int headeri = 0;
    while (1) {
        if (strncmp(msg + pos, crlf, strlen(crlf)) == 0) {
            return pos + strlen(crlf);
        }

        char* delim = strchr(msg + pos, ':');
        if (delim == NULL) {
            return -1;
        }
        int keyLen = delim - (msg + pos);
        struct header* parsedHeader = calloc(1, sizeof(struct header));
        parsedHeader->key = calloc(keyLen+1, sizeof(char));
        strncpy(parsedHeader->key, msg + pos, keyLen);
        pos += keyLen + 2;

        char* endOfVal = strchr(msg + pos, '\r');
        if (endOfVal == NULL) {
            free(parsedHeader->key);
            free(parsedHeader);
            return -1;
        }
        int  lenOnVal = endOfVal - (msg + pos);
        parsedHeader->value = calloc(lenOnVal + 1, sizeof(char));
        strncpy(parsedHeader->value, msg + pos, lenOnVal);
        pos += lenOnVal;
        if (strncmp(msg + pos, crlf, strlen(crlf)) != 0) {
            free(parsedHeader->key);
            free(parsedHeader->value);
            free(parsedHeader);
            return -1;

        }
        pos += strlen(crlf);
        headers[headeri]  = parsedHeader;
        headeri++;
    }
}

void free_httpRequest(struct httpRequest* req) {
    free(req->HTTPMethode);
    free(req->route);
    free(req->payload);
    for (int i= 0; req->headers[i] != NULL; i++) {
        free(req->headers[i]->key);
        free(req->headers[i]->value);
        free(req->headers[i]);
    }

    free(req);
}
char* append_strings(char* str1, char* str2) {
    if (str1 == NULL) {
        if (str2 == NULL) return calloc(1, 1);
        return str2;
    }
    if (str2 == NULL) return str1;
    char* res = malloc(strlen(str1) + strlen(str2) + 1);
    if (res == NULL) return NULL;
    strncpy(res, str1, strlen(str1));
    strncpy(res+strlen(str1), str2, strlen(str2));
    return res;
}
