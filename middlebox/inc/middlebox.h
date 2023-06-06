#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <inttypes.h>
#include <unistd.h>
#include <time.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_cycles.h>
#include <rte_lcore.h>
#include <rte_mbuf.h>
#include <rte_byteorder.h>
#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_arp.h>
#include <rte_hash.h>
#include <rte_jhash.h>

#include <config.h>

// mempool is defined here as a global struct so every function have access to it
extern struct rte_mempool *mbuf_pool;
extern struct rte_mempool *small_mbuf_pool;

struct ip_tuple {
	uint32_t src_ip;
	uint32_t dst_ip;
	uint16_t src_port;
	uint16_t dst_port;
};

struct net_sge {
	void *payload;
	uint32_t len;
	void *handle; // Not to be used by applications
};

/* Port Initialisation */

int port_init(uint16_t port, struct rte_mempool *mbuf_pool);

/* Utilities */

void ip_addr_to_str(uint32_t addr, char *str);

uint32_t get_local_ip(void);

void get_local_mac(struct rte_ether_addr *mac);

void pkt_dump(struct rte_mbuf *pkt);

int ip_is_multicast(uint32_t ip);

uint32_t ip_str_to_int(const char *ip);

int str_to_eth_addr(const char *src, unsigned char *dst);

/* Datalink Layer - Ethernet */

void eth_in(struct rte_mbuf *pkt_buf);

int eth_out(struct rte_mbuf *pkt_buf, uint16_t h_proto,
			struct rte_ether_addr *dst_haddr, uint16_t iplen);

int dpdk_eth_send(struct rte_mbuf *pkt_buf, uint16_t len);

/* ARP Handling */

void arp_in(struct rte_mbuf *pkt_buf, struct rte_arp_hdr *arph);

void arp_out(struct rte_mbuf *pkt_buf, struct rte_arp_hdr *arph, int opcode,
					uint32_t dst_ip, struct rte_ether_addr *dst_haddr);

void init_arp_table();

int add_arp_entry(const char *ip, const char *mac);

struct rte_ether_addr *arp_lookup_mac(uint32_t addr);

/* Network Layer - IP */

void ip_in(struct rte_mbuf *pkt_buf, struct rte_ipv4_hdr *iph);

void ip_out(struct rte_mbuf *pkt_buf, struct rte_ipv4_hdr *iph, uint32_t src_ip,
			uint32_t dst_ip, uint8_t ttl, uint8_t tos, uint8_t proto,
			uint16_t l4len, struct rte_ether_addr *dst_haddr);

/* Transport Layer - UDP */

void udp_in(struct rte_mbuf *pkt_buf, struct rte_ipv4_hdr *iph,
			struct rte_udp_hdr *udph);

int udp_send(struct net_sge *e, uint32_t destination_ip);

