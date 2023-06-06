#include <middlebox.h>

void ip_addr_to_str(uint32_t addr, char *str)
{
	snprintf(str, 16, "%d.%d.%d.%d", ((addr >> 24) & 0xff),
			 ((addr >> 16) & 0xff), ((addr >> 8) & 0xff), (addr & 0xff));
} 

void pkt_dump(struct rte_mbuf *pkt)
{
	struct rte_ether_hdr *ethh = rte_pktmbuf_mtod(pkt, struct rte_ether_hdr *);
	struct rte_ipv4_hdr *iph = rte_pktmbuf_mtod_offset(pkt, struct rte_ipv4_hdr *,
												   sizeof(struct rte_ether_hdr));
	struct rte_udp_hdr *udph = rte_pktmbuf_mtod_offset(pkt, struct rte_udp_hdr *,
												   sizeof(struct rte_ether_hdr) +
													   sizeof(struct rte_ipv4_hdr));

	printf("DST MAC: ");
	for (int i = 0; i < RTE_ETHER_ADDR_LEN; i++)
		printf("%hhx ", (char)ethh->dst_addr.addr_bytes[i]);
	printf(" SRC MAC: ");
	for (int i = 0; i < RTE_ETHER_ADDR_LEN; i++)
		printf("%hhx ", (char)ethh->src_addr.addr_bytes[i]);
	//printf("\n");

	char ipaddr[64];
	ip_addr_to_str(iph->src_addr, ipaddr);
	printf("SRC IP: %s ", ipaddr);
	ip_addr_to_str(iph->dst_addr, ipaddr);
	printf("DST IP: %s ", ipaddr);
	printf("\n");
}

int ip_is_multicast(uint32_t ip)
{
	uint32_t first_oct;

	first_oct = ip & 0xFF;
	return  (first_oct >= 224) && (first_oct <= 239);
}

uint32_t ip_str_to_int(const char *ip)
{
	uint32_t addr;
	unsigned char a, b, c, d;
	if (sscanf(ip, "%hhu.%hhu.%hhu.%hhu", &a, &b, &c, &d) != 4) {
		return -EINVAL;
	}
	addr = RTE_IPV4(a, b, c, d);
	return addr;
}

int str_to_eth_addr(const char *src, unsigned char *dst)
{
	struct rte_ether_addr tmp;

	if (sscanf(src, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", &tmp.addr_bytes[0],
			   &tmp.addr_bytes[1], &tmp.addr_bytes[2], &tmp.addr_bytes[3],
			   &tmp.addr_bytes[4], &tmp.addr_bytes[5]) != 6)
		return -EINVAL;
	memcpy(dst, &tmp, sizeof(tmp));
	return 0;
}

uint32_t get_local_ip(void)
{
  return ip_str_to_int(LOCAL_IP); // equivalent to 10.1.0.2
}

void get_local_mac(struct rte_ether_addr *mac)
{
	//rte_eth_macaddr_get(0, mac); // Assume only one NIC
	rte_ether_unformat_addr(LOCAL_MAC,mac);
}
