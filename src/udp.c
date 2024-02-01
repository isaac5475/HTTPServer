//
// Created by murat on 27.12.23.
//

#include <stdio.h>
#include <sys/types.h>
#include "udp.h"
void udp_handler(uint8_t* buff, int fd, struct sockaddr* ipaddr, socklen_t addr_len, struct data *data, struct dht* dht) {
    uint8_t offset = 0;
    uint8_t msg_type_received = buff[0];
    offset++;

    uint16_t hash_id_received;
    memcpy(&hash_id_received, buff + offset, sizeof(uint16_t));
    hash_id_received = ntohs(hash_id_received);
    offset += sizeof(uint16_t);

    uint16_t node_id_received;
    memcpy(&node_id_received, buff + offset, sizeof(uint16_t)); //node_id
    node_id_received = ntohs(node_id_received);
    offset += sizeof(uint16_t);

    struct in_addr node_ip_received;
    memcpy(&node_ip_received, buff + offset, sizeof(in_addr_t)); //node_id
    offset += 4; //node ip
    char node_ip_received_str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &node_ip_received, node_ip_received_str, INET_ADDRSTRLEN);

    uint16_t node_port_received;
    memcpy(&node_port_received, buff + offset, sizeof(uint16_t)); //node_id
    node_port_received = ntohs(node_port_received);

    if (msg_type_received == LOOKUP_MODE) {
        //if we are node responsible for resource;
        if (successor_is_responsible(data, hash_id_received) == 1) {
            uint8_t reply_packet[11];
            uint16_t hash_id = data->dhtInstance->node_id;
            uint16_t node_id = data->dhtInstance->succ_id;

            struct in_addr node_ip;
            if (inet_pton(AF_INET, data->dhtInstance->succ_ip, &node_ip) <= 0) {
                perror("inet_pton");
            }
            uint16_t node_port = data->dhtInstance->succ_port;
            create_msg(reply_packet, REPLY_MODE, hash_id, node_id, &node_ip, node_port);
            struct sockaddr_in addr_to_send;
            memset(&addr_to_send, 0, sizeof(addr_to_send));
            addr_to_send.sin_family = AF_INET;
            addr_to_send.sin_port = htons(node_port_received);
            addr_to_send.sin_addr = node_ip_received;
            send_msg(fd, reply_packet, &addr_to_send);
        } else {
            forward_packet(buff, fd, dht);    //current node isn't responsible for resource, pass forward
        }

    } else if (msg_type_received == REPLY_MODE) {
        uint8_t i = data->oldest_record;
        data->oldest_record = (data->oldest_record + 1) % 10;
        data->hash_records[i]->hash_id = hash_id_received;
        data->hash_records[i]->node_id = node_id_received;
        sprintf(data->hash_records[i]->host, "%s:%d", node_ip_received_str, node_port_received);

    } else if (msg_type_received == STABILIZE_MODE) {
            uint8_t reply_packet[11];
            uint16_t hash_id = 0;
            uint16_t node_id = data->dhtInstance->prev_node_id;

            struct in_addr node_ip;
            if (inet_pton(AF_INET, data->dhtInstance->prev_ip, &node_ip) <= 0) {
                perror("inet_pton");
            }
            uint16_t node_port = data->dhtInstance->prev_port;
            create_msg(reply_packet, NOTIFY_MODE, hash_id, node_id, &node_ip, node_port);
            struct sockaddr_in addr_to_send;
            memset(&addr_to_send, 0, sizeof(addr_to_send));
            addr_to_send.sin_family = AF_INET;
            addr_to_send.sin_port = htons(node_port_received);
            addr_to_send.sin_addr = node_ip_received;

            send_msg(fd, reply_packet, &addr_to_send);

    } else if (msg_type_received == NOTIFY_MODE) {
        if (node_id_received != data->dhtInstance->node_id) {//New node has entered DHT, which is successor of current node
            data->dhtInstance->node_id = node_id_received;
            memset(data->dhtInstance->node_ip, 0, sizeof(*data->dhtInstance->node_ip));
            memcpy(data->dhtInstance->node_ip, node_ip_received_str, sizeof(node_ip_received));
            data->dhtInstance->node_port = node_port_received;
        }
    } else if (msg_type_received == JOIN_MODE) {
        if (node_is_responsible(data, node_id_received)) {
            uint8_t reply_packet[11];
            uint16_t hash_id = 0;
            uint16_t node_id = data->dhtInstance->node_id;

            struct in_addr node_ip;
            if (inet_pton(AF_INET, data->dhtInstance->node_ip, &node_ip) <= 0) {
                perror("inet_pton");
            }
            uint16_t node_port = data->dhtInstance->node_port;
            create_msg(reply_packet, NOTIFY_MODE, hash_id, node_id, &node_ip, node_port);
            struct sockaddr_in addr_to_send;
            memset(&addr_to_send, 0, sizeof(addr_to_send));
            addr_to_send.sin_family = AF_INET;
            addr_to_send.sin_port = htons(node_port_received);
            addr_to_send.sin_addr = node_ip_received;

            send_msg(fd, reply_packet, &addr_to_send);

            data->dhtInstance->prev_node_id = node_id_received;
            data->dhtInstance->prev_ip = node_ip_received_str;
            data->dhtInstance->prev_port = node_port_received;

        } else {
            forward_packet(buff, fd, dht);
        }
    }

}

void forward_packet(const uint8_t *buff, int fd, const struct dht *dht) {
    struct sockaddr_in node_addr;
    memset(&node_addr, 0, sizeof(node_addr));

    node_addr.sin_family = AF_INET;
    node_addr.sin_port = htons(dht->succ_port);
    inet_pton(AF_INET, dht->succ_ip, &node_addr.sin_addr);
    sendto(fd, buff,11, 0, (struct sockaddr*)&node_addr, sizeof(node_addr));
}

void create_msg(uint8_t* packet, uint8_t msg_type, uint16_t hash_id, uint16_t node_id, struct in_addr* node_ip, uint16_t node_port) {
    packet[0] = msg_type;
    int offset = 1;
    uint16_t hashN = htons(hash_id);
    memcpy(packet + offset, &hashN, sizeof(uint16_t));
    offset += sizeof(uint16_t);

    uint16_t node_idN = htons(node_id);
    memcpy(packet + offset, &node_idN, sizeof(uint16_t));
    offset += sizeof(uint16_t);
    memcpy(packet + offset, &node_ip->s_addr, 4);
    offset += 4;

    uint16_t node_port_n = htons(node_port);
    memcpy(packet + offset, &node_port_n, sizeof(uint16_t));

}

void send_msg(int fd, uint8_t* msg, struct sockaddr_in* node_addr) {
    sendto(fd, msg, 11, 0, (struct sockaddr*)node_addr, sizeof(*node_addr));
}

uint8_t successor_is_responsible(const struct data *data, uint16_t id) {
    if ((id <= data->dhtInstance->succ_id && id > data->dhtInstance->node_id)
    || (data->dhtInstance->node_id > data->dhtInstance->succ_id
    && (id > data->dhtInstance->node_id
    || id <= data->dhtInstance->succ_id))) {
        return 1;
    } else {
        return 0;
    }
}

void send_stabilize_msg(int fd, const struct data *data) {
    uint8_t stabilize_packet[11];
    uint16_t hash_id = data->dhtInstance->node_id;
    uint16_t node_id = data->dhtInstance->node_id;

    struct in_addr node_ip;
    if (inet_pton(AF_INET, data->dhtInstance->node_ip, &node_ip) <= 0) {
        perror("inet_pton");
    }
    uint16_t node_port = data->dhtInstance->node_port;
    create_msg(stabilize_packet, STABILIZE_MODE, hash_id, node_id, &node_ip, node_port);
    struct sockaddr_in addr_to_send;
    memset(&addr_to_send, 0, sizeof(addr_to_send));
    addr_to_send.sin_family = AF_INET;
    addr_to_send.sin_port = htons(data->dhtInstance->succ_port);
    inet_pton(AF_INET, data->dhtInstance->succ_ip, &addr_to_send.sin_addr);

    send_msg(fd, stabilize_packet, &addr_to_send);
}
