#include <middlebox.h>
#include <api.h>

//Internal helper function which fills in IP header.
struct ip_tuple *init_ip_tuple(struct rte_mbuf *pkt_buf, uint32_t destination_ip) {
  
  struct ip_tuple *id = rte_pktmbuf_mtod(pkt_buf, struct ip_tuple *);
  
  id->src_ip = ip_str_to_int(LOCAL_IP);
  id->dst_ip = destination_ip;
  id->src_port = LOCAL_PORT;
  id->dst_port = TARGET_PORT;
  return id;
}

//Internal helper function which fills in packet fields prior to passing to IP layer.
int udp_out(struct rte_mbuf *pkt_buf, struct ip_tuple *id, int len)
{
  struct rte_udp_hdr *udph = rte_pktmbuf_mtod_offset(pkt_buf, struct rte_udp_hdr *,
						     sizeof(struct rte_ether_hdr) +
						     sizeof(struct rte_ipv4_hdr));
  struct rte_ipv4_hdr *iph = rte_pktmbuf_mtod_offset(pkt_buf, struct rte_ipv4_hdr *,
						     sizeof(struct rte_ether_hdr));  
  udph->dgram_cksum = 0;
  udph->dgram_len = rte_cpu_to_be_16(len + sizeof(struct rte_udp_hdr));
  udph->src_port = rte_cpu_to_be_16(id->src_port);
  udph->dst_port = rte_cpu_to_be_16(id->dst_port);
  
  struct rte_ether_addr dst_haddr;
  ip_out(pkt_buf, iph, id->src_ip, id->dst_ip, 64, 0, IPPROTO_UDP,
	 len + sizeof(struct rte_udp_hdr), &dst_haddr);
  return 0;
}

//Passes information from packet to application layer.
void udp_in(struct rte_mbuf *pkt_buf, struct rte_ipv4_hdr *iph,
			struct rte_udp_hdr *udph){

  // Drop packets from unknown addresses or if sent to wrong port
  u_int32_t src_addr = rte_be_to_cpu_32(iph->src_addr);
  if (!arp_lookup_mac(src_addr) || 
      rte_be_to_cpu_16(udph->dst_port) != LOCAL_PORT) {
	  
	  rte_pktmbuf_free(pkt_buf);
	  return;
  }
  
  // Pass replies from server to application layer
  if (src_addr == ip_str_to_int(TARGET_IP)) {
    app_server_in(pkt_buf, udph);
    return;   
  }
  
  // Pass requests from client to application layer
  app_client_in(pkt_buf, udph, src_addr);
  return;
} 

//Sends a packet to destination_ip. To be called by API methods.
int udp_send(struct net_sge *e, uint32_t destination_ip){
  
  struct ip_tuple *id = init_ip_tuple(e->handle, destination_ip);

  if (e->len > UDP_MAX_LEN) {
    printf("Message length too long!\n");
    return -1;
  }
  
  return udp_out(e->handle, id, e->len);
};


