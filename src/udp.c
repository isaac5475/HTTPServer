//
// Created by murat on 27.12.23.
//

#include <stdio.h>
#include "udp.h"
void udp_handler(char* msg_buf) {
    printf("listener: packet contains \"%s\"\n", msg_buf);

}
