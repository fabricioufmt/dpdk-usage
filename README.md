# dpdk-usage

Experiment 01 (ex1 directory):
- Topology:
	VM-Sender --- VM-Middlebox --- VM-Receiver
- Experiment:
	VM-Sender and VM-Receiver use PktGen-DPDK to send and receive packets;
	VM-Middlebox receives packets from network, changes address values, and reinjects them into network using DPDK.
- Files:
	ex01
	ex01.c
	ex01.pdf
	basicfwd.c
	sender-conf
	receiver-conf
	pktgen-dpdk-usage



