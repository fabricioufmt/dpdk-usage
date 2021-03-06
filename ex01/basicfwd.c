/*-
 * dasd  BSD LICENSE
 *
 *   Copyright(c) 2010-2015 Intel Corporation. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdint.h>
#include <inttypes.h>
#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_cycles.h>
#include <rte_lcore.h>
#include <rte_mbuf.h>

#include <rte_byteorder.h>
#include <rte_ip.h>
#include <rte_icmp.h>
#include <rte_arp.h>

#define RX_RING_SIZE 128
#define TX_RING_SIZE 512

#define NUM_MBUFS 8191
#define MBUF_CACHE_SIZE 250
#define BURST_SIZE 32
#define MYIP IPv4(10, 0, 0, 100)

static const struct rte_eth_conf port_conf_default = {
	.rxmode = { .max_rx_pkt_len = ETHER_MAX_LEN }
};

/* basicfwd.c: Basic DPDK skeleton forwarding example. */

static inline uint16_t checksum(void* buf, size_t len) {
  uint32_t sum;
  uint16_t* u16 = (uint16_t*) buf;

  sum = 0;
  while(len >= (sizeof(*u16) * 4)) {
    sum += rte_be_to_cpu_16(u16[0]);
    sum += rte_be_to_cpu_16(u16[1]);
    sum += rte_be_to_cpu_16(u16[2]);
    sum += rte_be_to_cpu_16(u16[3]);
    len -= sizeof(*u16) * 4;
    u16 += 4;
  }

  while(len >= sizeof(*u16)) {
    sum += rte_be_to_cpu_16(*u16);
    len -= sizeof(*u16);
    u16 += 1;
  }

  if(len == 1)
    sum += *((const uint8_t*) u16);

  sum = ((sum & 0xffff0000) >> 16) + (sum & 0xffff);
  sum = ((sum & 0xffff0000) >> 16) + (sum & 0xffff);

  return sum;
}
/*
 * Initializes a given port using global settings and with the RX buffers
 * coming from the mbuf_pool passed as a parameter.
 */
static inline int port_init(uint8_t port, struct rte_mempool *mbuf_pool) {
  struct rte_eth_conf port_conf = port_conf_default;
  const uint16_t rx_rings = 1, tx_rings = 1;
  int retval;
  uint16_t q;
  
  if (port >= rte_eth_dev_count())
    return -1;
  
  /* Configure the Ethernet device. */
  retval = rte_eth_dev_configure(port, rx_rings, tx_rings, &port_conf);
  if (retval != 0)
    return retval;
  
  /* Allocate and set up 1 RX queue per Ethernet port. */
  for (q = 0; q < rx_rings; q++) {
    retval = rte_eth_rx_queue_setup(port, q, RX_RING_SIZE, rte_eth_dev_socket_id(port), NULL, mbuf_pool);
    if (retval < 0)
      return retval;
  }
  
  /* Allocate and set up 1 TX queue per Ethernet port. */
  for (q = 0; q < tx_rings; q++) {
    retval = rte_eth_tx_queue_setup(port, q, TX_RING_SIZE, rte_eth_dev_socket_id(port), NULL);
    if (retval < 0)
      return retval;
  }
  
  /* Start the Ethernet port. */
  retval = rte_eth_dev_start(port);
  if (retval < 0)
    return retval;
  
  /* Display the port MAC address. */
  struct ether_addr addr;
  rte_eth_macaddr_get(port, &addr);
  printf("Port %u MAC: %02" PRIx8 " %02" PRIx8 " %02" PRIx8
	 " %02" PRIx8 " %02" PRIx8 " %02" PRIx8 "\n",
	 (unsigned)port,
	 addr.addr_bytes[0], addr.addr_bytes[1],
	 addr.addr_bytes[2], addr.addr_bytes[3],
	 addr.addr_bytes[4], addr.addr_bytes[5]);
  
  /* Enable RX in promiscuous mode for the Ethernet device. */
  rte_eth_promiscuous_enable(port);
  
  return 0;
}

/*
 * The lcore main. This is the main thread that does the work, reading from
 * an input port and writing to an output port.
 */
static __attribute__((noreturn)) void lcore_main(void) {
  const uint8_t nb_ports = rte_eth_dev_count();
  uint8_t port;
  uint16_t i;
  struct ether_hdr* ethdr;
  struct ipv4_hdr* iphdr; 
  struct icmp_hdr* icmphdr;
  struct arp_hdr* arphdr;

  /*
   * Check that the port is on the same NUMA node as the polling thread
   * for best performance.
   */
  for (port = 0; port < nb_ports; port++)
    if (rte_eth_dev_socket_id(port) > 0 && rte_eth_dev_socket_id(port) != (int)rte_socket_id())
      printf("WARNING, port %u is on remote NUMA node to "
	     "polling thread.\n\tPerformance will "
	     "not be optimal.\n", port);

  printf("\nCore %u forwarding packets. [Ctrl+C to quit]\n",
	 rte_lcore_id());

  /* Run until the application is quit or killed. */
  for (;;) {
    for (port = 0; port < nb_ports; port++) {

      /* Get burst of RX packets, from first port of pair. */
      struct rte_mbuf *bufs[BURST_SIZE];
      const uint16_t nb_rx = rte_eth_rx_burst(port, 0, bufs, BURST_SIZE);

      for(i = 0; i < nb_rx; i++) {
	ethdr = (struct ether_hdr*) rte_pktmbuf_mtod(bufs[i], struct ether_hdr*);
	RTE_ASSERT(ethdr != NULL);
	
	if(unlikely(ethdr->ether_type == rte_cpu_to_be_16(ETHER_TYPE_ARP))) {
	  arphdr = (struct arp_hdr*) rte_pktmbuf_mtod_offset(bufs[i], struct arp_hdr*, sizeof(struct ether_hdr));
	  RTE_ASSERT(arphdr != NULL);
	  if(likely(arphdr->arp_op == rte_cpu_to_be_16(ARP_OP_REQUEST))) {
	    arphdr->arp_op = rte_cpu_to_be_16(ARP_OP_REPLY);
	    memcpy(&ethdr->d_addr, &ethdr->s_addr, sizeof(ethdr->s_addr));
	    rte_eth_macaddr_get(port, &ethdr->s_addr);
	    memcpy(&arphdr->arp_data.arp_tha, &arphdr->arp_data.arp_sha, sizeof(arphdr->arp_data.arp_sha));
	    arphdr->arp_data.arp_tip = arphdr->arp_data.arp_sip;
	    memcpy(&arphdr->arp_data.arp_sha, &ethdr->s_addr, sizeof(ethdr->s_addr));
	    arphdr->arp_data.arp_sip = rte_cpu_to_be_32(MYIP);
	  }
	} else if(likely(ethdr->ether_type == rte_cpu_to_be_16(ETHER_TYPE_IPv4))) {
	  iphdr = (struct ipv4_hdr*) rte_pktmbuf_mtod_offset(bufs[i], struct ipv4_hdr*, sizeof(struct ether_hdr));
	  RTE_ASSERT(iphdr != NULL);
	  
	  if(unlikely(iphdr->next_proto_id == IPPROTO_ICMP)) {
	    icmphdr = (struct icmp_hdr*) rte_pktmbuf_mtod_offset(bufs[i], struct ipv4_hdr*, sizeof(struct ether_hdr) + sizeof(struct ipv4_hdr));
	    RTE_ASSERT(icmphdr != NULL);

	    memcpy(&ethdr->d_addr, &ethdr->s_addr, sizeof(ethdr->s_addr));
	    rte_eth_macaddr_get(port, &ethdr->s_addr);

	    iphdr->packet_id = iphdr->packet_id + 1;
	    uint32_t auxip = iphdr->src_addr;
	    iphdr->src_addr = iphdr->dst_addr;
	    iphdr->dst_addr = auxip;

	    if(icmphdr->icmp_type == IP_ICMP_ECHO_REQUEST) {
	      icmphdr->icmp_type = IP_ICMP_ECHO_REPLY;
	    } 
	    
	    uint16_t iphdrlen = (iphdr->version_ihl & IPV4_HDR_IHL_MASK) * IPV4_IHL_MULTIPLIER;

	    icmphdr->icmp_cksum = 0;
	    uint16_t icmpcksum = rte_raw_cksum(icmphdr, rte_be_to_cpu_16(iphdr->total_length) - iphdrlen);
	    icmphdr->icmp_cksum = icmpcksum == 0xFFFF ? icmpcksum : ~icmpcksum;
	  }
	  
	  iphdr->hdr_checksum = 0;
	  iphdr->hdr_checksum = rte_ipv4_cksum(iphdr); 
	}
      }
      if (unlikely(nb_rx == 0))
	continue;

      /* Send burst of TX packets, to second port of pair. */
      const uint16_t nb_tx = rte_eth_tx_burst(port, 0, bufs, nb_rx);

      /* Free any unsent packets. */
      if (unlikely(nb_tx < nb_rx)) {
      	uint16_t buf;
      	for (buf = nb_tx; buf < nb_rx; buf++)
      		rte_pktmbuf_free(bufs[buf]);
      }
    }
  }
}

/*
 * The main function, which does initialization and calls the per-lcore
 * functions.
 */
int
main(int argc, char *argv[])
{
	struct rte_mempool *mbuf_pool;
	unsigned nb_ports;
	uint8_t portid;

	/* Initialize the Environment Abstraction Layer (EAL). */
	int ret = rte_eal_init(argc, argv);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Error with EAL initialization\n");

	argc -= ret;
	argv += ret;

	/* Check that there is an even number of ports to send/receive on. */
	nb_ports = rte_eth_dev_count();
	//if (nb_ports < 2 || (nb_ports & 1))
	//	rte_exit(EXIT_FAILURE, "Error: number of ports must be even\n");

	/* Creates a new mempool in memory to hold the mbufs. */
	mbuf_pool = rte_pktmbuf_pool_create("MBUF_POOL", NUM_MBUFS * nb_ports,
		MBUF_CACHE_SIZE, 0, RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());

	if (mbuf_pool == NULL)
		rte_exit(EXIT_FAILURE, "Cannot create mbuf pool\n");

	/* Initialize all ports. */
	for (portid = 0; portid < nb_ports; portid++)
		if (port_init(portid, mbuf_pool) != 0)
			rte_exit(EXIT_FAILURE, "Cannot init port %"PRIu8 "\n",
					portid);

	if (rte_lcore_count() > 1)
		printf("\nWARNING: Too many lcores enabled. Only 1 used.\n");

	/* Call lcore_main on the master core only. */
	lcore_main();

	return 0;
}
