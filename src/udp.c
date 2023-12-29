//
// Created by murat on 27.12.23.
//

#include <stdio.h>
#include <sys/types.h>
#include "udp.h"
void udp_handler(uint8_t* buff, int fd, struct sockaddr* ipaddr, socklen_t addr_len, struct data *data, struct dht* dht) {
    uint8_t offset = 0;
    uint8_t mode = *buff;
    offset++;

    uint16_t hash;
    memcpy(&hash, buff + offset, sizeof(uint16_t));
    hash = ntohs(hash);
    offset += sizeof(uint16_t);

    uint16_t currNode;
    memcpy(&currNode, buff + offset, sizeof(uint16_t)); //node_id
    currNode = ntohs(currNode);
    offset += sizeof(uint16_t);

    offset += sizeof(in_addr_t); //node ip

    offset += sizeof(uint16_t); //node port

    if ((hash <= dht->succ_id && hash > data->dhtInstance->node_id)
        || (data->dhtInstance->node_id > data->dhtInstance->succ_id && (hash > data->dhtInstance->node_id || hash <= data->dhtInstance->succ_id))) { //prevnode < hashed < node_id
        uint8_t buf[11];

        uint16_t hash_idN = htons(data->dhtInstance->node_id); //prepare for transmitting;

        buf[0] = 1;
        memcpy(buf+ 1, &hash_idN, 2);  //id of succ node

        uint16_t node_idN = htons(data->dhtInstance->succ_id);
        memcpy(buf+ 3, &node_idN, 2);  //id of succ node

        struct in_addr ip_address;
        // Convert string to binary form (4 bytes)
        if (inet_pton(AF_INET, data->dhtInstance->succ_ip, &ip_address) <= 0) {
            perror("inet_pton");
        }
        memcpy(buf + 5, &ip_address.s_addr, 4); //ipaddr of succ node
        uint16_t succ_portN = htons(data->dhtInstance->succ_port);
        memcpy(buf+ 9, &succ_portN, 2);  //port of succ node
        int bytes_sent = sendto(fd, buf, sizeof(buf), 0, ipaddr, addr_len);

    }

}
