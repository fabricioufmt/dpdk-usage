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

#include <signal.h>

#define RX_RING_SIZE 128
#define TX_RING_SIZE 512

#define NUM_MBUFS 8191
#define MBUF_CACHE_SIZE 250
#define BURST_SIZE 32

uint32_t t0, r0, t1, r1;

void signal_handler(int);

void signal_handler(int signal) {
  printf("\n\n");
  printf("\t\t==========Statistics==========\n");
  printf("\t\t\tRx/Tx [packets] on port 0: %d/%d\n", r0, t0);
  printf("\t\t\tRx/Tx [packets] on port 1: %d/%d\n", r1, t1);
  printf("\n\n");
  signal += 0;
  exit(0);
}

static const struct rte_eth_conf port_conf_default = {
	.rxmode = { .max_rx_pkt_len = ETHER_MAX_LEN }
};

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
  printf("\tEnabling promiscuous mode on port %u.\n", port);
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
  //struct ipv4_hdr* iphdr; 
  //struct tcp_hdr* icmphdr;

  /*
   * Check that the port is on the same NUMA node as the polling thread
   * for best performance.
   */
  for (port = 0; port < nb_ports; port++)
    if (rte_eth_dev_socket_id(port) > 0 && rte_eth_dev_socket_id(port) != (int)rte_socket_id())
      printf("WARNING, port %u is on remote NUMA node to polling thread.\n\tPerformance will not be optimal.\n", port);

  printf("Setting value 0 on Rx/Tx counters.\n");
  r0 = r1 = t0 = t1 = 0;
  
  printf("\nCore %u forwarding packets. [Ctrl+C to quit]\n", rte_lcore_id());
  printf("\tfrom port %u to port %u\n", 0, 0 ^ 1);
  printf("\tfrom port %u to port %u\n", 0 ^ 1, 0);
  
  /* Run until the application is quit or killed. */
  for (;;) {
    //for (port = 0; port < nb_ports; port++) {
    for (port = 0; port < 2; port++) {

      /* Get burst of RX packets, from first port of pair. */
      struct rte_mbuf *bufs[BURST_SIZE];
      const uint16_t nb_rx = rte_eth_rx_burst(port, 0, bufs, BURST_SIZE);

      if (unlikely(nb_rx == 0))
	continue;

      for(i = 0; i < nb_rx; i++) {
	ethdr = (struct ether_hdr*) rte_pktmbuf_mtod(bufs[i], struct ether_hdr*);
	RTE_ASSERT(ethdr != NULL);

	/*printf("from: \n");
	printf("\t Port: %u --", port);
	printf(
	       " %02" PRIx8 " %02" PRIx8 " %02" PRIx8
	       " %02" PRIx8 " %02" PRIx8 " %02" PRIx8 " ->"
	       " %02" PRIx8 " %02" PRIx8 " %02" PRIx8
	       " %02" PRIx8 " %02" PRIx8 " %02" PRIx8 "\n",
	       ethdr->s_addr.addr_bytes[0], ethdr->s_addr.addr_bytes[1],
	       ethdr->s_addr.addr_bytes[2], ethdr->s_addr.addr_bytes[3],
	       ethdr->s_addr.addr_bytes[4], ethdr->s_addr.addr_bytes[5],
	       ethdr->d_addr.addr_bytes[0], ethdr->d_addr.addr_bytes[1],
	       ethdr->d_addr.addr_bytes[2], ethdr->d_addr.addr_bytes[3],
	       ethdr->d_addr.addr_bytes[4], ethdr->d_addr.addr_bytes[5]);
	printf("to: \n");
	printf("\t Port: %u --", port ^ 1);*/
	if(ethdr->d_addr.addr_bytes[4] == 0xff && ethdr->d_addr.addr_bytes[5] == 0x01) {
	  //00:01 -> ff:01 => ff:02 -> 00:02
	  ethdr->s_addr.addr_bytes[3] = 0xff;
	  ethdr->s_addr.addr_bytes[4] = 0xff;
	  ethdr->s_addr.addr_bytes[5] = 0x02;
	  ethdr->d_addr.addr_bytes[3] = 0x00;
	  ethdr->d_addr.addr_bytes[4] = 0x00;
	  ethdr->d_addr.addr_bytes[5] = 0x02;
	} else if(ethdr->d_addr.addr_bytes[4] == 0xff && ethdr->d_addr.addr_bytes[5] == 0x02) {
	  //00:02 -> ff:02 => ff:01 -> 00:01
	  ethdr->s_addr.addr_bytes[3] = 0xff;
	  ethdr->s_addr.addr_bytes[4] = 0xff;
	  ethdr->s_addr.addr_bytes[5] = 0x01;
	  ethdr->d_addr.addr_bytes[3] = 0x00;
	  ethdr->d_addr.addr_bytes[4] = 0x00;
	  ethdr->d_addr.addr_bytes[5] = 0x01;
	}
	/*printf(
	       " %02" PRIx8 " %02" PRIx8 " %02" PRIx8
	       " %02" PRIx8 " %02" PRIx8 " %02" PRIx8 " ->"
	       " %02" PRIx8 " %02" PRIx8 " %02" PRIx8
	       " %02" PRIx8 " %02" PRIx8 " %02" PRIx8 "\n\n",
	       ethdr->s_addr.addr_bytes[0], ethdr->s_addr.addr_bytes[1],
	       ethdr->s_addr.addr_bytes[2], ethdr->s_addr.addr_bytes[3],
	       ethdr->s_addr.addr_bytes[4], ethdr->s_addr.addr_bytes[5],
	       ethdr->d_addr.addr_bytes[0], ethdr->d_addr.addr_bytes[1],
	       ethdr->d_addr.addr_bytes[2], ethdr->d_addr.addr_bytes[3],
	       ethdr->d_addr.addr_bytes[4], ethdr->d_addr.addr_bytes[5]);*/
      }
	
      /* Send burst of TX packets, to second port of pair. */
      const uint16_t nb_tx = rte_eth_tx_burst(port ^ 1, 0, bufs, nb_rx);

      if(port == 0) {
	r0 += nb_rx;
	t1 += nb_tx;
      } else {
	r1 += nb_rx;
	t0 += nb_tx;
      }
      
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
int main(int argc, char *argv[]) {
  struct rte_mempool *mbuf_pool;
  unsigned nb_ports;
  uint8_t portid;

  struct sigaction sa;
  sa.sa_handler = &signal_handler;
  if(sigaction(SIGTERM, &sa, NULL) == -1) {
    perror("Error on handle SIGTERM");
  }
  if(sigaction(SIGINT, &sa, NULL) == -1) {
    perror("Error on handle SIGTINT");
  }
  
  /* Initialize the Environment Abstraction Layer (EAL). */
  int ret = rte_eal_init(argc, argv);
  if (ret < 0)
    rte_exit(EXIT_FAILURE, "Error with EAL initialization\n");

  argc -= ret;
  argv += ret;

  /* Check that there is an even number of ports to send/receive on. */
  nb_ports = rte_eth_dev_count();

  /* Creates a new mempool in memory to hold the mbufs. */
  mbuf_pool = rte_pktmbuf_pool_create("MBUF_POOL", NUM_MBUFS * nb_ports, MBUF_CACHE_SIZE, 0, RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());

  if (mbuf_pool == NULL)
    rte_exit(EXIT_FAILURE, "Cannot create mbuf pool\n");

  /* Initialize all ports. */
  for (portid = 0; portid < nb_ports; portid++)
    if (port_init(portid, mbuf_pool) != 0)
      rte_exit(EXIT_FAILURE, "Cannot init port %"PRIu8 "\n", portid);

  if (rte_lcore_count() > 1)
    printf("\nWARNING: Too many lcores enabled. Only 1 used.\n");

  /* Call lcore_main on the master core only. */
  lcore_main();

  return 0;
}
