/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2015 Intel Corporation
 */
#include <middlebox.h>
#include <api.h>

struct rte_mempool* mbuf_pool;
struct rte_mempool *small_mbuf_pool;

static __rte_noreturn void lcore_main(void)
{
	uint16_t port;
	int counter = 0;
	printf("Middlebox is currently listening...\n");
	while (true) 
	{
		// Iterate for all assigned port to listen for packets
		RTE_ETH_FOREACH_DEV(port) 
		{
			refresh_backlog();
			struct rte_mbuf *bufs[BURST_SIZE];
			const uint16_t nb_rx = rte_eth_rx_burst(port, 0, bufs, BURST_SIZE);
			if (nb_rx != 0)
			{
			//   counter++;
			//   printf("\n%u. Received %u packets!!: \n",counter,nb_rx);
				for (int i = 0; i < nb_rx; i++) {
					// printf("buf-%i addr: %i\n",i,bufs[i]);
					refresh_backlog();
					eth_in(bufs[i]);
				}
			}
		}
	}
}


int main(int argc, char *argv[])
{
	
	unsigned nb_ports;
	uint16_t portid;

	// Initializing EAL (Environment Abstraction Layer)
	int ret = rte_eal_init(argc, argv);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Error with EAL initialization\n");
	argc -= ret;
	argv += ret;

	// Printing the number of ports available
	nb_ports = rte_eth_dev_count_avail();
	printf("Number of ports: %u\n",nb_ports);

	// Allocating new mempool for mbuf
	mbuf_pool = rte_pktmbuf_pool_create("MBUF_POOL", NUM_MBUFS * nb_ports,
		MBUF_CACHE_SIZE, 0, RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());
	if (mbuf_pool == NULL)
		rte_exit(EXIT_FAILURE, "Cannot create mbuf pool\n");

	small_mbuf_pool = rte_pktmbuf_pool_create("SMALL_MBUF_POOL", NUM_SMALL_MBUFS * nb_ports,
		MBUF_CACHE_SIZE, 0, SMALL_MBUFS_SIZE, rte_socket_id());
	if (small_mbuf_pool == NULL)
		rte_exit(EXIT_FAILURE, "Cannot create small mbuf pool\n");

	// Initializing ports
	RTE_ETH_FOREACH_DEV(portid)
	{
		if (port_init(portid, mbuf_pool) != 0)
			rte_exit(EXIT_FAILURE, "Cannot init port %"PRIu16 "\n",
					portid);
		printf("Port %u Initialized\n", portid);
	}

	// fill up arp table
	init_arp_table();

	init_hashmap();
	
	lcore_main();

	rte_eal_cleanup();

	return 0;
}
