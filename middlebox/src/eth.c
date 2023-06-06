#include <middlebox.h>

void eth_in(struct rte_mbuf *pkt_buf)
{
	unsigned char *payload = rte_pktmbuf_mtod(pkt_buf, unsigned char *);
	struct rte_ether_hdr *hdr = (struct rte_ether_hdr *)payload;
	struct rte_arp_hdr *arph;
	struct rte_ipv4_hdr *iph;

	if (hdr->ether_type == rte_cpu_to_be_16(RTE_ETHER_TYPE_ARP)) {
		arph = (struct rte_arp_hdr *)(payload + (sizeof(struct rte_ether_hdr)));
		arp_in(pkt_buf, arph);
	}  else if (hdr->ether_type == rte_be_to_cpu_16(RTE_ETHER_TYPE_IPV4)) {
		iph = (struct rte_ipv4_hdr *)(payload + (sizeof(struct rte_ether_hdr)));
		ip_in(pkt_buf, iph);
	}  else {
	  //	printf("Unknown ether type: %" PRIu16 "\n",
	  //	   rte_be_to_cpu_16(hdr->ether_type));
	}
}

int eth_out(struct rte_mbuf *pkt_buf, uint16_t h_proto,
			struct rte_ether_addr *dst_haddr, uint16_t iplen)
{
	/* fill the ethernet header */
	struct rte_ether_hdr *hdr = rte_pktmbuf_mtod(pkt_buf, struct rte_ether_hdr *);

	hdr->dst_addr = *dst_haddr;
	get_local_mac(&hdr->src_addr);
	hdr->ether_type = rte_cpu_to_be_16(h_proto);

	/* enqueue the packet */
	return dpdk_eth_send(pkt_buf, iplen + sizeof(struct rte_ether_hdr));
}

int dpdk_eth_send(struct rte_mbuf *pkt_buf, uint16_t len)
{
	int ret = 0;

	/* get mbuf from user data */
	pkt_buf->pkt_len = len;
	pkt_buf->data_len = len;

	while (1) {
		ret = rte_eth_tx_burst(0, 0, &pkt_buf, 1);
		if (ret == 1)
			break;
	}
	
	return 1;
}
