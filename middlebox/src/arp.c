#include <middlebox.h>

struct arp_entry {
	uint32_t addr;
	struct rte_ether_addr mac;
};

uint16_t arp_count = 0;

struct arp_entry known_haddrs[ARP_ENTRIES_COUNT];

void arp_in(struct rte_mbuf *pkt_buf, struct rte_arp_hdr *arph)
{
	/* process only arp for this address */
	if (rte_be_to_cpu_32(arph->arp_data.arp_tip) != get_local_ip()) {
		printf("ARP request for (not me) %i\n",rte_be_to_cpu_32(arph->arp_data.arp_tip));
		return;
	}

	switch (rte_be_to_cpu_16(arph->arp_opcode)) {
	case RTE_ARP_OP_REQUEST:
		arp_out(pkt_buf, arph, RTE_ARP_OP_REPLY, arph->arp_data.arp_sip,
				&arph->arp_data.arp_sha);
		break;
	case RTE_ARP_OP_REPLY:
		printf("received ARP reply");
		break;
	default:
		printf("apr: Received unknown ARP op");
		break;
	}
}


void arp_out(struct rte_mbuf *pkt_buf, struct rte_arp_hdr *arph, int opcode,
					uint32_t dst_ip, struct rte_ether_addr *dst_haddr)
{
	int sent;

	/* fill arp header */
	/* previous fields remain the same */
	arph->arp_opcode = rte_cpu_to_be_16(opcode);

	/* fill arp body */
	arph->arp_data.arp_sip = rte_cpu_to_be_32(get_local_ip());
	arph->arp_data.arp_tip = dst_ip;

	arph->arp_data.arp_tha = *dst_haddr;
	get_local_mac(&arph->arp_data.arp_sha); // Assume only one NIC

	//printf("ARP request replied!\n");
	sent = eth_out(pkt_buf, RTE_ETHER_TYPE_ARP, &arph->arp_data.arp_tha,
				   sizeof(struct rte_arp_hdr));
	assert(sent == 1);
}


struct rte_ether_addr *arp_lookup_mac(uint32_t addr)
{

	int i;
	for (i = 0; i < ARP_ENTRIES_COUNT; i++) {
		if (addr == known_haddrs[i].addr)
			return &known_haddrs[i].mac;
	}
	return NULL;
}


int add_arp_entry(const char *ip, const char *mac)
{
	int ret;
	printf("Adding IP: %s MAC: %s\n", ip, mac);
	if (arp_count >= ARP_ENTRIES_COUNT) {
		fprintf(stderr, "Not enough space for new arp entry\n");
		return -1;
	}
	known_haddrs[arp_count].addr = ip_str_to_int(ip);
	// ret = str_to_eth_addr(mac, (unsigned char *)&known_haddrs[arp_count++].mac);
	// if (ret) {
	// 	fprintf(stderr, "Error parsing marc\n");
	// 	return -1;
	// }

	rte_ether_unformat_addr(mac, &(known_haddrs[arp_count++].mac));

	return 0;
}

void init_arp_table() {
  FILE * fp = fopen("config-files/arp-data.csv", "r");

  if (fp == NULL) {
    printf("ARP-Data cannot be opened!\n");
    exit(EXIT_FAILURE);
  }

  char* line = NULL;
  size_t len = 0;
  ssize_t read;

  while ((read = getline(&line, &len, fp)) != -1) {
	// find occurence of space separator
	char* ptr = line;
	while(*ptr != ' ') {
		ptr++;
	}

	char* ip_ptr = line;
	char* mac_ptr = ptr + 1;

	// replace space separator as sentinel char
	*ptr = '\0';

	// replace \n as sentinel char
	ptr = mac_ptr;
	while(*ptr && *ptr != '\n')
		ptr++;
	*ptr = '\0';

	add_arp_entry(ip_ptr, mac_ptr);
  }

  free(line);
  fclose(fp);
}
