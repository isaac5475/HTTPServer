//
// Created by murat on 27.12.23.
//

#ifndef RN_PRAXIS_UDP_H
#define RN_PRAXIS_UDP_H

#include "utils.h"

#define LOOKUP_MODE 0
#define REPLY_MODE 1
#define NOTIFY_MODE 3
#define JOIN_MODE 4
void udp_handler(uint8_t* buff, int fd, struct sockaddr* ipaddr, socklen_t addr_len, struct data* data, struct dht* dht);
void create_msg(uint8_t* packet, uint8_t msg_type, uint16_t hash_id, uint16_t node_id, struct in_addr* node_ip, uint16_t node_port);
void send_msg(int fd, uint8_t* msg, struct sockaddr_in* node_addr);
void forward_packet(const uint8_t *buff, int fd, const struct dht *dht);
uint8_t successor_is_responsible(const struct data *data, uint16_t id);
#endif //RN_PRAXIS_UDP_H
