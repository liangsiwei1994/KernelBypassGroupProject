#pragma once

#define RX_RING_SIZE 1024
#define TX_RING_SIZE 1024

#define NUM_MBUFS 8191
#define NUM_SMALL_MBUFS 1000
#define SMALL_MBUFS_SIZE 256
#define MAP_ENTRIES 600 // should be less than NUM_MBUFS
#define MBUF_CACHE_SIZE 250
#define BURST_SIZE 32
#define ETH_MTU 1500
#define UDP_MAX_LEN (ETH_MTU - sizeof(struct rte_ipv4_hdr) - sizeof(struct rte_udp_hdr))
#define ARP_ENTRIES_COUNT 64

#define LOCAL_MAC "DE:AD:BE:EF:AA:AA"
#define LOCAL_IP "10.1.0.2"
#define LOCAL_PORT 8080

#define TARGET_MAC "DE:AD:BE:EF:8B:16"
#define TARGET_IP "10.1.0.3"
#define TARGET_PORT 8080


