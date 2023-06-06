#pragma once

#include <middlebox.h>

#define MEMCACHED_REQ_ID_SIZE 2
#define MEMCACHED_HEADER "\0\0\0\1\0\0"
#define MEMCACHED_HEADER_SIZE 6
#define MEMCACHED_TAIL "\r\n"
#define MEMCACHED_TAIL_SIZE 2

#define MAX_KEY_PER_PACKET 722 //Assuming client sends identical 1-byte long keys
#define MAX_KEY_LENGTH 1443
#define MIN_OUTSTANDING_REQ 1000
#define MAX_REQ_ID 60000

#define MAX_PAYLOAD_LEN (UDP_MAX_LEN - MEMCACHED_REQ_ID_SIZE - MEMCACHED_HEADER_SIZE - MEMCACHED_TAIL_SIZE)
#define GETS_LEN 4
#define FULFILL_THRESHOLD 0.5
#define TIMEOUT 100 //ms

#define PROGRAM_TIMEOUT -1 //determine how long the middlebox will run in ms (-1 for indefinite runtime)

/* Application Programming Interface */
void init_hashmap();

void app_server_in(struct rte_mbuf *pkt_buf, struct rte_udp_hdr *udph);

void app_client_in(struct rte_mbuf *pkt_buf, struct rte_udp_hdr *udph, uint32_t client_address);

void refresh_backlog();
