//
// Created by murat on 11/11/23.
//

#include <fcntl.h>
#include "utils.h"

int start_server_tcp(char* ipaddr, char* port, int* sockfd) {
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
        fcntl(*sockfd, F_SETFL, O_NONBLOCK);
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

int start_server_udp(char* ipaddr, char* port, int* sockfd) {
    struct addrinfo hints, *servinfo, *p;
    int yes = 1;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
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
        fcntl(*sockfd, F_SETFL, O_NONBLOCK);
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


void request_handler(int fd, char* msgPrefix, struct data* data) {
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
        char* newMsg = calloc(REQUEST_LEN, 1);
        if (strlen(msgPrefix) != 0) {
            strncat(newMsg, msgPrefix, strlen(msgPrefix));
            memset(msgPrefix, 0, REQUEST_LEN);
            strncat(newMsg, msgBuf, strlen(msgBuf));
        } else {
            strncpy(newMsg, msgBuf, strlen(msgBuf));
        }
        int position = 0;
        struct httpRequest** requests = calloc(10, sizeof(struct httpRequest*));
        int parse_res = parse_requests(newMsg, requests, &position);
        if (position != strlen(newMsg)) { //if we couldn't parse the end of the incoming message, save it till next time
            size_t remainingLength = strlen(newMsg) - position;
            strncpy(msgPrefix, newMsg + position, remainingLength);
            msgPrefix[remainingLength] = '\0';
        } else if (requests[parse_res-1]->status == -2) {
            strncpy(msgPrefix, newMsg, strlen(newMsg));
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
            if (requests[i]->status == -2) {
                free_httpRequest((requests[i]));
                continue;
            }

            uint16_t hashed = hash(requests[i]->route);
            if ((hashed <= data->node_id && hashed > data->dhtInstance->prev_node_id)
            || (data->dhtInstance->prev_node_id > data->node_id && (hashed > data->dhtInstance->prev_node_id || hashed <= data->dhtInstance->node_id))) { //prevnode < hashed < node_id
                if (strncmp(requests[i]->HTTPMethode, "GET", strlen("GET")) == 0) {
                    get_handler(fd, requests[i], data->dynamicResources);
                    free_httpRequest((requests[i]));
                    continue;
                }
                if (strncmp(requests[i]->HTTPMethode, "PUT", strlen("PUT")) == 0) {
                    put_handler(fd, requests[i], data->dynamicResources);
                    free_httpRequest((requests[i]));
                    continue;
                }
                if (strncmp(requests[i]->HTTPMethode, "DELETE", strlen("DELETE")) == 0) {
                    delete_handler(fd, requests[i], data->dynamicResources);
                    free_httpRequest((requests[i]));
                    continue;
                }
                send(fd, STATUS_CODE_501, strlen(STATUS_CODE_501), 0);
                free_httpRequest(requests[i]);
            } else {
                char buf[256];
                sprintf(buf, "HTTP/1.1 303 See Other\r\nContent-Length: 0\r\nLocation: http://%s:%d%s\r\n\r\n", data->dhtInstance->succ_ip, data->dhtInstance->succ_port, requests[i]->route);
                send(fd, buf, strlen(buf), 0);
            }

        }
        free(requests);
//        return;
    }
}

void put_handler(int fd, struct httpRequest *req, struct dynamicResource* dynamicResources[MAX_RESOURCES_AMOUNT]) {
//    send(fd, STATUS_CODE_404, strlen(STATUS_CODE_404), 0);
//    return;
    printf("start put_handler\n");
    int content_len_val = -1;
    const char clkey[] = "Content-Length";
    for (int i = 0; i < MAX_HEADERS_AMOUNT && req->headers[i] != NULL; i++) {
        if (strncmp(req->headers[i]->key, clkey, strlen(clkey)) == 0) {
            content_len_val = strtol(req->headers[i]->value, NULL, 10);
            if (content_len_val == 0) {//Couldn't parse or header == 0, which is invalid behaviour
                send(fd, STATUS_CODE_400_CL, strlen(STATUS_CODE_400_CL), 0);
                return;
            } else break;
        }
    }
    if (req->payload == NULL) {
    send(fd, STATUS_CODE_403, strlen(STATUS_CODE_403), 0);
    return;}
    if (content_len_val == -1 || req->payload == NULL ) { //wasn't found
        send(fd, STATUS_CODE_400_CL, strlen(STATUS_CODE_400_CL), 0);
        return;
    }
    if (strncmp(req->route, DYNAMIC_ROUTE, strlen(DYNAMIC_ROUTE)) == 0) {
        int res = add_dynamic_record(req->route, req->payload, dynamicResources);
        if (res == 0) {//success
            send(fd,STATUS_CODE_201, strlen(STATUS_CODE_201), 0);
        } else {    //overriden
            send(fd, STATUS_CODE_204, strlen(STATUS_CODE_204), 0);
        }
    } else {
        send(fd, STATUS_CODE_403, strlen(STATUS_CODE_403), 0);
    }
}

void delete_handler(int fd, struct httpRequest *req, struct dynamicResource* dynamicResources[MAX_RESOURCES_AMOUNT]) {
    if (strncmp(req->route, DYNAMIC_ROUTE, strlen(DYNAMIC_ROUTE)) == 0) {
        int res = delete_dynamic_record(req->route, dynamicResources);
        if (res == -1) {
            send(fd, STATUS_CODE_404, strlen(STATUS_CODE_404), 0);
            return;
        }
        send(fd, STATUS_CODE_204, strlen(STATUS_CODE_204), 0);
    } else {
        send(fd, STATUS_CODE_403, strlen(STATUS_CODE_403), 0);
    }
}



void get_handler(int fd, struct httpRequest *req, struct dynamicResource* dynamicResources[MAX_RESOURCES_AMOUNT]) {
    const char foo_url[] = "/static/foo";
    const char bar_url[] = "/static/bar";
    const char baz_url[] = "/static/baz";
    if (strncmp(req->route, foo_url, strlen(foo_url)) == 0) {
        char* resp = "HTTP/1.1 200 OK\r\nContent-Length: 3\r\n\r\nFoo";
        send(fd, resp, strlen(resp), 0);
    } else if (strncmp(req->route, bar_url, strlen(bar_url)) == 0) {
        char* resp = "HTTP/1.1 200 OK\r\nContent-Length: 3\r\n\r\nBar";
        send(fd, resp, strlen(resp), 0);
    } else if (strncmp(req->route, baz_url, strlen(baz_url)) == 0) {
        char *resp = "HTTP/1.1 200 OK\r\nContent-Length: 3\r\n\r\nBaz";
        send(fd, resp, strlen(resp), 0);

    } else if (strncmp(req->route, DYNAMIC_ROUTE, strlen(DYNAMIC_ROUTE)) == 0) {
        char* payload = read_dynamic_record(req->route, dynamicResources);
      char resp[REQUEST_LEN];
      if (payload == NULL) {
          send(fd,STATUS_CODE_404_CL, strlen(STATUS_CODE_404_CL), 0);
          return;
      }
        sprintf(resp, "HTTP/1.1 200 OK\r\nContent-Length: %d\r\n\r\n%s", (int)strlen(payload), payload);
        send(fd, resp, strlen(resp), 0);
    } else {
        send(fd, STATUS_CODE_404, strlen(STATUS_CODE_404), 0);
    }
}

char* read_dynamic_record(char* requestedRoute, struct dynamicResource* dynamicResources[MAX_RESOURCES_AMOUNT]) {//NULL on failure, char* to value otherwise
    for (int i = 0; i < MAX_RESOURCES_AMOUNT && dynamicResources[i] != NULL; i++) {
        if (strncmp(dynamicResources[i]->key, requestedRoute, strlen(requestedRoute)) == 0) {
            return dynamicResources[i]->value;
        }
    }
    return NULL;
}

int add_dynamic_record(char* requestedRoute, char* payload, struct dynamicResource* dynamicResources[MAX_RESOURCES_AMOUNT]) {// 0 on success, -1 if not
    int i = 0;
    while (dynamicResources[i] != NULL) {i++;}
    dynamicResources[i] = malloc(sizeof(struct dynamicResource));
    if (dynamicResources[i] == NULL) return -1;
    dynamicResources[i]->key = calloc(sizeof(char) * strlen(requestedRoute) + 1, sizeof(char));
    if (dynamicResources[i]->key == NULL) return -1;
    dynamicResources[i]->value = calloc(sizeof(char) * strlen(payload) + 1, sizeof(char));
    if (dynamicResources[i]->value == NULL) return -1;
    strncpy(dynamicResources[i]->key, requestedRoute, strlen(requestedRoute));
    strncpy(dynamicResources[i]->value, payload, strlen(payload));
    return 0;
}

int delete_dynamic_record(char * requestedRoute, struct dynamicResource* dynamicResources[MAX_RESOURCES_AMOUNT]) {
    for (int i = 0; i < MAX_RESOURCES_AMOUNT && dynamicResources[i] != NULL; i++) {
        if (strncmp(dynamicResources[i]->key, requestedRoute, strlen(requestedRoute)) == 0) {
            free(dynamicResources[i]->key);
            free(dynamicResources[i]->value);
            free(dynamicResources[i]);
            dynamicResources[i] = NULL;
            return 0;
        }
    }
    return -1;
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
            free(parsedReq);
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
        req->HTTPVersion = (char *) calloc(1, strlen(httpVersion) + 1);
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
    if (contentLen > strlen(msg+pos)) {//payload may be not received as whole
        req->status = -2;//incomplete
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
    free(req->HTTPVersion);
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
        if (str2 == NULL) return NULL;
        return str2;
    }
    if (str2 == NULL) return str1;
    char* res = calloc(strlen(str1) + strlen(str2) + 1, sizeof(char));
    if (res == NULL) return NULL;
    strncpy(res, str1, strlen(str1));
    strncpy(res+strlen(str1), str2, strlen(str2));
    return res;
}

void free_dynamic_records(struct dynamicResource* dynamicResources[MAX_RESOURCES_AMOUNT]) {
    for (int i = 0; i < MAX_RESOURCES_AMOUNT && dynamicResources[i] != NULL; i++) {
        free(dynamicResources[i]->key);
        free(dynamicResources[i]->value);
    }
}

uint16_t hash(const char* str) {
     uint8_t digest[SHA256_DIGEST_LENGTH];
     SHA256((uint8_t *)str, strlen(str), digest);
     return htons(*((uint16_t *)digest)); // We only use the first two bytes here
     }
int get_prev_node_id() {
    char* res = getenv("SUCC_ID");
    if (res == NULL) {
        return -1;
    }
    char* s;
    int id = strtol(res, &s, 10);
    return id;
}
int populate_dht_struct(struct dht* dht) {
   char* pred_id = getenv("PRED_ID");
   if (pred_id == NULL) {
       dht->prev_node_id = 0;
   } else {
       dht->prev_node_id = atol(pred_id);
   }
   char* pred_ip = getenv("PRED_IP");
   if (pred_ip != NULL) {
       dht->prev_ip = calloc(1, strlen(pred_ip) + 1);
       strncpy(dht->prev_ip, pred_ip, strlen(pred_ip));
   }
   char* pred_port = getenv("PRED_PORT");
   if (pred_port != NULL) {
       dht->prev_port = atol(pred_port);
   }
   char* succ_id = getenv("SUCC_ID");
   if (succ_id != NULL) {
       dht->succ_id = atol(succ_id);
   }
   char* succ_ip = getenv("SUCC_IP");
   if (succ_ip != NULL) {
       dht->succ_ip = calloc(1, strlen(succ_ip) + 1);
       strncpy(dht->succ_ip, succ_ip, strlen(succ_ip));
   }
   char* succ_port = getenv("SUCC_PORT");
   if (succ_port != NULL) {
       dht->succ_port = atol(succ_port);
   }
    return 0;
}
