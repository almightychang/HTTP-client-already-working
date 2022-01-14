#ifndef __TCP_RESTCLIENT_H
#define __TCP_RESTCLIENT_H

#include "lwip/tcp.h"


#define DEST_IP_ADDR0   (uint8_t) 20
#define DEST_IP_ADDR1   (uint8_t) 127
#define DEST_IP_ADDR2   (uint8_t) 54
#define DEST_IP_ADDR3   (uint8_t) 216

#define DEST_PORT       (uint16_t) 80


#define HTTPC_REQ_11 "POST /register_device/ HTTP/1.1\r\n" /* URI */\
    "Host: 20.127.54.216\r\n" /* User-Agent */ \
    "User-Agent: lwip\r\n" \
    "Content-Type: application/json\r\n" /* we don't support persistent connections, yet */ \
    "Accept: */*\r\n" \
    "Content-Length: 38\r\n" \
    "\r\n" \
    "{\"serial\" : 10123, \"pk_store\" : 2}\r\n" \
    "\r\n"

err_t tcp_restclient_connect(void);
void test(void);

#endif
