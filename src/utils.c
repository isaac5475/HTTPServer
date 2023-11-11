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

void request_handler(int fd) {
    char msg[REQUEST_LEN];
    memset(msg, 0, REQUEST_LEN);
    if (recv(fd, msg, REQUEST_LEN, 0) == -1) {
        perror("read");
    }
    struct httpRequest request;
    int parse_res = parse_request(msg, &request);
    if (parse_res == -1) {
        send(fd, STATUS_CODE_400, strlen(STATUS_CODE_400), 0);
        return;
    }

    if (strncmp(request.HTTPMethode, "GET", strlen("GET")) == 0) {
        send(fd, STATUS_CODE_404, strlen(STATUS_CODE_404), 0);
        return;
    }
    if (strncmp(request.HTTPMethode, "PUT", strlen("PUT")) == 0) {
        int content_len_val = -1;
        const char clkey[] = "Content-Length";
        for (int i = 0; i < MAX_HEADERS_AMOUNT && request.headers[i] != NULL; i++) {

            if (strncmp(request.headers[i], clkey, strlen(clkey)) == 0) {
                content_len_val = strtol(request.headers[i]->value, NULL, 10);
                if (content_len_val == 0) {//Couldn't parse or header == 0, which is invalid behaviou
                    send(fd, STATUS_CODE_400, strlen(STATUS_CODE_400), 0);
                    return;
                } else break;
            }
        }
        if (content_len_val == -1) { //wasn't found
            send(fd, STATUS_CODE_400, strlen(STATUS_CODE_400), 0);
            return;
        }
    }//end of PUT part
    send(fd, STATUS_CODE_501, strlen(STATUS_CODE_501), 0);
    return;

}


int parse_request(char* msg, struct httpRequest* req) {
    int pos = 0;
    int method_is_parsed = 
    if (strncmp(msg, "GET", strlen("GET")) == 0) { //   parse method
        pos += strlen("GET");
        req->HTTPMethode = (char *) calloc(1, sizeof("GET"));
        strncpy(req->HTTPMethode, msg, strlen("GET"));
    } else {
        req->status = -1;
        return -1;
    }

    if (strncmp(msg, "GET", strlen("GET")) == 0) { //   parse method
        pos += strlen("GET");
        req->HTTPMethode = (char *) calloc(1, sizeof("GET"));
        strncpy(req->HTTPMethode, msg, strlen("GET"));
    } else {
        req->status = -1;
        return -1;
    }

    pos += 1; // ' '
    long long lenOfUrl = strchr(msg + pos, ' ') - (msg + pos);  //  parse URL
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
    struct header* headers[MAX_HEADERS_AMOUNT];
    if (parse_headers(msg + pos, headers) == -1) {
        req->status = -1;
        return -1;
    }

    return 1;


}

int parse_headers(char* msg, struct header* headers[]) {
    int pos = 0;
    for (int i = 0; i < MAX_HEADERS_AMOUNT; i++) {
       headers[i] = NULL;
    }
    char crlf[] = "\r\n";
    int headeri = 0;
    while (1) {
        if (strncmp(msg + pos, crlf, strlen(crlf)) == 0) {
            return headeri == 0 ? -1 : 1;
        }

        char* delim = strchr(msg + pos, ':');
        if (delim == NULL) {
            return -1;
        }
        int keyLen = delim - (msg + pos);
        struct header* parsedHeader = calloc(1, sizeof(struct header));
        parsedHeader->key = calloc(keyLen, sizeof(char));
        strncpy(parsedHeader->key, msg + pos, keyLen);
        pos += keyLen + 1;

        char* endOfVal = strchr(msg + pos, '\r');
        if (endOfVal == NULL) {
            free(parsedHeader->key);
            free(parsedHeader);
            return -1;
        }
        int  lenOnVal = endOfVal - (msg + pos);
        parsedHeader->value = calloc(lenOnVal, sizeof(char));
        strncpy(parsedHeader->value, msg + pos, lenOnVal);
        pos += lenOnVal;
        if (strncmp(msg + pos, crlf, strlen(crlf)) != 0) {
            free(parsedHeader->key);
            free(parsedHeader->value);
            free(parsedHeader);
            return -1;

        }
        pos += strlen(crlf);
        headers[headeri++] = parsedHeader;
    }
}
