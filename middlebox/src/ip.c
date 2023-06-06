#include <middlebox.h>

void ip_in(struct rte_mbuf *pkt_buf, struct rte_ipv4_hdr *iph)
{
	// struct icmp_hdr *icmph;
	struct rte_udp_hdr *udph;
	// struct igmpv2_hdr *igmph;
	int hdrlen;

	if ((iph->dst_addr != rte_cpu_to_be_32(get_local_ip()))
			&& !ip_is_multicast(iph->dst_addr))
		goto out;

	/* perform necessary checks */
	hdrlen = (iph->version_ihl & RTE_IPV4_HDR_IHL_MASK) * RTE_IPV4_IHL_MULTIPLIER;

	switch (iph->next_proto_id) {
	case IPPROTO_TCP:
	  //printf("TCP not supported\n");
		rte_pktmbuf_free(pkt_buf);
		break;
	case IPPROTO_UDP:
		udph = (struct rte_udp_hdr *)((unsigned char *)iph + hdrlen);
		udp_in(pkt_buf, iph, udph);
		break;
	// case IPPROTO_ICMP:
	// 	icmph = (struct icmp_hdr *)((unsigned char *)iph + hdrlen);
	// 	icmp_in(pkt_buf, iph, icmph);
	// 	break;
	// case IPPROTO_IGMP:
	// 	igmph = (struct igmpv2_hdr *)((unsigned char *)iph + hdrlen);
	// 	igmp_in(pkt_buf, iph, igmph);
	// 	break;
	default:
		goto out;
	}
	return;

out:
	//printf("UNKNOWN L3 PROTOCOL OR WRONG DST IP\n");
	rte_pktmbuf_free(pkt_buf);
}


void ip_out(struct rte_mbuf *pkt_buf, struct rte_ipv4_hdr *iph, uint32_t src_ip,
			uint32_t dst_ip, uint8_t ttl, uint8_t tos, uint8_t proto,
			uint16_t l4len, struct rte_ether_addr *dst_haddr)
{
	int sent, hdrlen;
	char *options;

	hdrlen = sizeof(struct rte_ipv4_hdr);

	/* setup ip hdr */
	iph->version_ihl =
		(4 << 4) | (hdrlen / RTE_IPV4_IHL_MULTIPLIER);
	iph->type_of_service = tos;
	iph->total_length = rte_cpu_to_be_16(hdrlen + l4len);
	iph->packet_id = 0;
	iph->fragment_offset = rte_cpu_to_be_16(0x4000); // Don't fragment
	iph->time_to_live = ttl;
	iph->next_proto_id = proto;
	iph->hdr_checksum = 0;
	iph->src_addr = rte_cpu_to_be_32(src_ip);
	iph->dst_addr = rte_cpu_to_be_32(dst_ip);

	dst_haddr = arp_lookup_mac(dst_ip);

	char tmp[64];
	if (!dst_haddr) {
		ip_addr_to_str(dst_ip, tmp);
		printf("Unknown mac for %s\n", tmp);
	}
	assert(dst_haddr != NULL);
	
	///* compute checksum */
	iph->hdr_checksum = rte_raw_cksum(iph, hdrlen);
	iph->hdr_checksum = (iph->hdr_checksum == 0xffff) ? iph->hdr_checksum : (uint16_t)~(iph->hdr_checksum);

	if (proto == IPPROTO_TCP) {
		assert(0);
	}

	sent = eth_out(pkt_buf, RTE_ETHER_TYPE_IPV4, dst_haddr,
				   rte_be_to_cpu_16(iph->total_length));
	assert(sent == 1);
}



