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

Experiments 02, 03, and 04 (click directory):
- 02: it opens 'f1a.dump' file, rewrites tcp source-tuple (from 149.207.99.128:80 to 1.1.1.1:8080), and saves them in 'saida.dump' file.

- 03: it uses DPDK elements to receive from interface-dpdk 0 and forward to interface-dpdk 1

- 04: it uses FromDPDKDevice element to receive packets from interface-dpdk 0, modifies them using IPRewriter element as Experiment 02 and, saves them in 'saidaDPDK.dump' file.

- Files:
	ex02.click
	ex03.click
	ex04.click
	f1a.dump
