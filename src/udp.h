//
// Created by murat on 27.12.23.
//

#ifndef RN_PRAXIS_UDP_H
#define RN_PRAXIS_UDP_H

#include "utils.h"

#define LOOKUP_MODE 0
#define RESULT_MODE 1
void udp_handler(uint8_t* buff, int fd, struct sockaddr* ipaddr, socklen_t addr_len, struct data* data, struct dht* dht);
#endif //RN_PRAXIS_UDP_H
