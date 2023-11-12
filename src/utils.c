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

void request_handler(int fd, struct llRoot* database) {
    while (1) {
        char msg[REQUEST_LEN];
        memset(msg, 0, REQUEST_LEN);
        int recvRes = recv(fd, msg, REQUEST_LEN, 0);
        if (recvRes == -1) {
            continue;
        }
        if (recvRes == 0) {
            return;
        }
        struct httpRequest request;
        int parse_res = parse_request(msg, &request);
        if (parse_res == -1) {
            send(fd, STATUS_CODE_400, strlen(STATUS_CODE_400), 0);
            continue;
        }

        if (strncmp(request.HTTPMethode, "GET", strlen("GET")) == 0) {
            get_handler(fd, &request);
            continue;
        }
        if (strncmp(request.HTTPMethode, "PUT", strlen("PUT")) == 0) {
            put_handler(fd, &request, database);
            continue;
        }
        if (strncmp(request.HTTPMethode, "DELETE", strlen("DELETE")) == 0) {
            delete_handler(fd, &request, database);
            continue;
        }

        send(fd, STATUS_CODE_501, strlen(STATUS_CODE_501), 0);
    }
}

void put_handler(int fd, struct httpRequest *req, struct llRoot* database) {
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
        return;
    }
    int pos = 0;
    char dynamicRoute[] = "/dynamic/";
    if (strncmp(req->route, dynamicRoute, strlen(dynamicRoute)) == 0) {
        pos += strlen(dynamicRoute);
        int rest = req->routeLen - strlen((dynamicRoute));
        char* key = req->route + pos;
        struct entryNode* existingNode = findNodeByKey(database, key, rest);
        if (existingNode == NULL) {
            saveInLL(database, key, rest, req->payload, strlen(req->payload));
            send(fd, STATUS_CODE_201, strlen(STATUS_CODE_201), 0);
            return;
        }
        free(existingNode->val);
        calloc(strlen(req->payload), 1);
        strncpy(existingNode->val, req->payload, strlen(req->payload));
        send(fd, STATUS_CODE_204, strlen(STATUS_CODE_204), 0);
        return;
    } else {
        send(fd, STATUS_CODE_403, strlen(STATUS_CODE_403), 0);
    }
}

void delete_handler(int fd, struct httpRequest *req, struct llRoot* database) {
    int pos = 0;
    const char dynamicRoute[] = "/dynamic/";
    if (strncmp(req->route, dynamicRoute, strlen(dynamicRoute)) == 0) {
        pos += strlen(dynamicRoute);
        int rest = req->routeLen - strlen((dynamicRoute));
        char *key = req->route + pos;
        int res = removeFromLLByKey(key, rest, database);
        if (res == -1) {
            send(fd, STATUS_CODE_404, strlen(STATUS_CODE_404), 0);
            return;
        }
        send(fd, STATUS_CODE_204, strlen(STATUS_CODE_204), 0);
    } else {
        send(fd, STATUS_CODE_403, strlen(STATUS_CODE_403), 0);
    }
}

struct entryNode* findNodeByKey(struct llRoot* start, char* key, size_t len) {
    for (struct entryNode* i = start->start; i != NULL; i = i->next) {
        if (strncmp(key, i->key, len) == 0) {
            return i;
        }
    }
    return NULL;
}
int saveInLL(struct llRoot* start, char* key, size_t key_len, char* val, size_t val_len) {
    struct entryNode* newNode = calloc(1, sizeof(struct entryNode));
    newNode->next = NULL;
    newNode->key = calloc(1, key_len);
    strncpy(newNode->key, key, key_len);
    newNode->val = calloc(1, val_len);
    strncpy(newNode->val, val, val_len);

    if (start->start == NULL) {
        start->start = newNode;
        return 1;
    }

    struct entryNode* t1 = start->start;
    struct entryNode* t2;
    while (t1 != NULL) {
        t2 = t1;
        t1 = t1->next;
    }
    t2->next = newNode;
    return 1;
}

int removeFromLLByKey(char* key, size_t keyLen, struct llRoot* start) {
    if (strncmp(start->start->key, key, keyLen) == 0) {
        struct entryNode* t3 = start->start->next;
        free(start->start->key);
        free(start->start->val);
        free(start->start);
        start->start = t3;
        return 1;
    }

    struct entryNode* t1, *t2;
    t1 = start->start;

    while(t1->next != NULL) {
      t2 = t1;
      t1 = t1->next;
      if (strncmp(t1->key, key, keyLen) == 0) {
          struct entryNode* t3 = t1->next;
          free(t1->key);
          free(t1->val);
          free(t1);
          t2->next = t3;
          return 1;
      }
    }
    return -1;
}

void get_handler(int fd, struct httpRequest *req) {
    char foo_url[] = "/static/foo";
    char bar_url[] = "/static/bar";
    char baz_url[] = "/static/baz";
    if (strncmp(req->route, foo_url, strlen(foo_url)) == 0) {
        char* resp = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\nFoo\r\n";
        send(fd, resp, strlen(resp), 0);
    } else if (strncmp(req->route, bar_url, strlen(bar_url)) == 0) {
        char* resp = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\nBar\r\n";
        send(fd, resp, strlen(resp), 0);
    } else if (strncmp(req->route, baz_url, strlen(baz_url)) == 0) {
        char* resp = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\nBaz\r\n";
        send(fd, resp, strlen(resp), 0);
    } else {
        send(fd, STATUS_CODE_404, strlen(STATUS_CODE_404), 0);
    }
}


int parse_request(char* msg, struct httpRequest* req) {
    int pos = 0;
    if (strncmp(msg, "GET", strlen("GET")) == 0) { //   parse method
        pos += strlen("GET");
        req->HTTPMethode = (char *) calloc(1, sizeof("GET"));
        strncpy(req->HTTPMethode, msg, strlen("GET"));
    } else if (strncmp(msg, "PUT", strlen("PUT")) == 0) {
        pos += strlen("PUT");
        req->HTTPMethode = (char *) calloc(1, sizeof("PUT"));
        strncpy(req->HTTPMethode, msg, strlen("PUT"));
    } else {
        req->status = -1;
        return -1;
    }

    pos += 1; // ' '
    long long lenOfUrl = strchr(msg + pos, ' ') - (msg + pos);  //  parse URL
    req->routeLen = lenOfUrl;
    req->route = (char *) calloc(1, lenOfUrl);
    strncpy(req->route, msg + pos, lenOfUrl);
    pos += lenOfUrl;

    pos += 1; // ' '

    const char httpVersion[] = "HTTP/1.1";
    if (strncmp(msg + pos, httpVersion, strlen(httpVersion)) == 0) { //   parse method
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
    pos += 2;
    int headersLen = parse_headers(msg + pos, req->headers);
    if ( headersLen == -1) {
        req->status = -1;
        return -1;
    }
    pos += headersLen;
    if (strncmp(msg + pos, crlf, strlen(crlf)) == 0) {    //no payload;
        req->payload = NULL;
        return 1;
    }
    char* endOfPayload = strstr(msg + pos, crlf);
    if (endOfPayload == NULL) {
        return 1;
    }
    size_t lenOfPayload = endOfPayload - (msg + pos);
    req->payload = malloc(lenOfPayload);
    strncpy(req->payload, msg + pos, lenOfPayload);

    return 1;


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
            return headeri == 0 ? -1 : pos + strlen(crlf);
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
