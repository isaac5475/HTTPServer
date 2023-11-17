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

void request_handler(int fd, int shmid) {
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
        struct httpRequest* requests[10];
        int parse_res = parse_requests(msg,  requests);
        if (parse_res == 0) {
            send(fd, STATUS_CODE_400, strlen(STATUS_CODE_400), 0);
            continue;
        }
        for (int i = 0; i < parse_res; i++) {

            if (parse_res == -1) {
                send(fd, STATUS_CODE_400, strlen(STATUS_CODE_400), 0);
                continue;
            }

            if (strncmp(requests[i]->HTTPMethode, "GET", strlen("GET")) == 0) {
                get_handler(fd, requests[i], shmid);
                continue;
            }
            if (strncmp(requests[i]->HTTPMethode, "PUT", strlen("PUT")) == 0) {
                put_handler(fd, requests[i], shmid);
                continue;
            }
            if (strncmp(requests[i]->HTTPMethode, "DELETE", strlen("DELETE")) == 0) {
                delete_handler(fd, requests[i], shmid);
                continue;
            }
            send(fd, STATUS_CODE_501, strlen(STATUS_CODE_501), 0);
            break;

        }
        return;
    }
}

void put_handler(int fd, struct httpRequest *req, int shmid) {
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
        void* shm = shmat(shmid, NULL, 0);
        struct entryNode* existingNode = findNodeByKey(shmid, key, rest);
        if (existingNode == NULL) {
            saveInLL(shmid, key, rest, req->payload, strlen(req->payload));
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

void delete_handler(int fd, struct httpRequest *req, int shmid) {
    int pos = 0;
    const char dynamicRoute[] = "/dynamic/";
    if (strncmp(req->route, dynamicRoute, strlen(dynamicRoute)) == 0) {
        pos += strlen(dynamicRoute);
        int rest = req->routeLen - strlen((dynamicRoute));
        char *key = req->route + pos;
        void* shm = shmat(shmid, NULL, 0);
        struct llRot* start = (struct llRoot*) shm + sizeof(int);
        int res = removeFromLLByKey(shmid, key, rest);
        if (res == -1) {
            send(fd, STATUS_CODE_404, strlen(STATUS_CODE_404), 0);
            return;
        }
        send(fd, STATUS_CODE_204, strlen(STATUS_CODE_204), 0);
    } else {
        send(fd, STATUS_CODE_403, strlen(STATUS_CODE_403), 0);
    }
}

struct entryNode* findNodeByKey(int shmid, char* key, size_t len) {
    void* shm = shmat(shmid, NULL, 0);
    int amountOfEntries = (int)shm;
    for (struct entryNode* i = shm; i < shm + amountOfEntries * sizeof(struct entryNode); i++) {
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

void get_handler(int fd, struct httpRequest *req, struct llRoot* database) {
    char foo_url[] = "/static/foo";
    char bar_url[] = "/static/bar";
    char baz_url[] = "/static/baz";
    char dynamic_url[] = "/dynamic/";
    if (strncmp(req->route, foo_url, strlen(foo_url)) == 0) {
        char* resp = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\nFoo";
        send(fd, resp, strlen(resp), 0);
    } else if (strncmp(req->route, bar_url, strlen(bar_url)) == 0) {
        char* resp = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\nBar";
        send(fd, resp, strlen(resp), 0);
    } else if (strncmp(req->route, baz_url, strlen(baz_url)) == 0) {
        char *resp = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\nBaz";
        send(fd, resp, strlen(resp), 0);
    } else if (strncmp(req->route, dynamic_url, strlen(dynamic_url)) == 0) {
      size_t keyLen = strlen(req->route + strlen(dynamic_url));
      struct entryNode* foundNode = findNodeByKey(database, req->route + strlen(dynamic_url), keyLen);
      if (foundNode == NULL) {
          send(fd, STATUS_CODE_404, strlen(STATUS_CODE_404), 0);
          return;
      }
        char resp[512];
        sprintf(resp, "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\n%s", foundNode->val);
        send(fd, resp, strlen(resp), 0);
    } else {
        send(fd, STATUS_CODE_404, strlen(STATUS_CODE_404), 0);
    }
}

int parse_requests(char* msg, struct httpRequest* reqs[10]) { //return value - -1 if error, amount of parsed reqs otherw
    int reqsi = 0;
    int pos = 0;
    while (1) {
        struct httpRequest* parsedReq = calloc(1, sizeof(struct httpRequest));
        int res = parse_request(msg + pos, parsedReq);
        if (res == -1) {
            reqs[reqsi++] = parsedReq;
            pos++;
            break;
        }
        if (res == 0) {
            break;
        }
        pos += res;
        reqs[reqsi++] = parsedReq;
        if (reqsi >= 10) break;
    }
    return reqsi;
}

int parse_request(char* msg, struct httpRequest* req) {
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
    }
    else {
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
        return pos + strlen(crlf);
    }
    char* endOfPayload = strstr(msg + pos, crlf);
    if (endOfPayload == NULL) {
        return -1;
    }
    size_t lenOfPayload = endOfPayload - (msg + pos);
    req->payload = malloc(lenOfPayload);
    strncpy(req->payload, msg + pos, lenOfPayload);

    return pos + lenOfPayload + strlen(crlf);


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
            return headeri == 0 ? 0 : pos + strlen(crlf);
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
