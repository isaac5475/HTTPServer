//
// Created by murat on 27.12.23.
//

#include <stdio.h>
#include <sys/types.h>
#include "udp.h"
void udp_handler(uint8_t* buff, int fd, struct sockaddr* ipaddr, socklen_t addr_len, struct data *data, struct dht* dht) {
    uint8_t offset = 0;
    uint8_t mode = buff[0];
    offset++;

    uint16_t hash;
    memcpy(&hash, buff + offset, sizeof(uint16_t));
    hash = ntohs(hash);
    offset += sizeof(uint16_t);

    uint16_t currNode;
    memcpy(&currNode, buff + offset, sizeof(uint16_t)); //node_id
    currNode = ntohs(currNode);
    offset += sizeof(uint16_t);

    struct in_addr their_addr;
    memcpy(&their_addr, buff + offset, sizeof(in_addr_t)); //node_id
    offset += 4; //node ip
    char their_addr_str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &their_addr, their_addr_str, INET_ADDRSTRLEN);

    uint16_t node_port;
    memcpy(&node_port, buff + offset, sizeof(uint16_t)); //node_id
    node_port = ntohs(node_port);

    if (mode == LOOKUP_MODE) {
        printf("in lookup mode");
        //if we are node responsible for resource;
        if ((hash <= dht->succ_id && hash > data->dhtInstance->node_id)
            || (data->dhtInstance->node_id > data->dhtInstance->succ_id && (hash > data->dhtInstance->node_id || hash <= data->dhtInstance->succ_id))) { //prevnode < hashed < node_id -> currNode is repsponsible
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

            struct sockaddr_in addr_to_send;
            memset(&addr_to_send, 0, sizeof(addr_to_send));
            addr_to_send.sin_family = AF_INET;
            addr_to_send.sin_port = htons(node_port);
            addr_to_send.sin_addr = their_addr;
            int bytes_sent = sendto(fd, buf, 11, 0, (struct sockaddr*)&addr_to_send, sizeof(addr_to_send));
            printf("reply sent, bytes: %d, to: %s\n", bytes_sent, their_addr_str);
        } else {
            struct sockaddr_in node_addr; //current node isn't responsible for resource, pass forward
            memset(&node_addr, 0, sizeof(node_addr));

            node_addr.sin_family = AF_INET;
            node_addr.sin_port = htons(dht->succ_port);
            inet_pton(AF_INET, dht->succ_ip, &node_addr.sin_addr);
            size_t buff_size = sizeof(buff);
            sendto(fd, buff,11, 0, (struct sockaddr*)&node_addr, sizeof(node_addr));
        }
    } else if (buff[0] == RESULT_MODE) {
        printf("got udp response, should add to table\r\n");
        printf("hash in msg is %d.\nand length: %d", (unsigned int)hash, 11);
        uint8_t i = data->oldest_record;
        data->oldest_record = (data->oldest_record + 1) % 10;
        data->hash_records[i]->hash_id = hash;
        data->hash_records[i]->node_id = currNode;
        sprintf(data->hash_records[i]->host, "%s:%d", their_addr_str, node_port);
    }
}
