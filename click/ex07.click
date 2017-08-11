// Setting up objects
AddressInfo(
	dpdkdev0	192.168.0.254	192.168.0.0/24	08:00:27:ff:ff:01,
	dpdkdev1	200.200.200.200			08:00:27:ff:ff:02
);

todev0 :: ToDPDKDevice(0)
todev1 :: ToDPDKDevice(1)

arpq_0 :: ARPQuerier(dpdkdev0) -> todev0
arpq_1 :: ARPQuerier(dpdkdev1) -> todev1

nat_el :: IPRewriter(pattern dpdkdev1 1025-65535 - - 0 1, drop)

FromDPDKDevice(0)
	// IP, ARP Requests, ARP Replies
	-> class0 :: Classifier(12/0800, 12/0806 20/0001, 12/0806 20/0002)

	// ===== IP Process =====
	-> Strip(14)
	-> CheckIPHeader(CHECKSUM false)
	-> check_ip0 :: IPClassifier(icmp, tcp or udp)

	// ===== ARP Requests Process =====
	class0[1] -> ARPResponder(dpdkdev0)
	-> todev0

	// ===== ARP Replies Process =====
	class0[2] -> [1]arpq_0

FromDPDKDevice(1)
	// IP, ARP Requests, ARP Replies
	-> class1 :: Classifier(12/0800, 12/0806 20/0001, 12/0806 20/0002)

	// ===== IP Process =====
	-> Strip(14)
	-> CheckIPHeader(CHECKSUM false)
	-> check_ip1 :: IPClassifier(icmp, tcp or udp)

	// ===== ARP Requests Process =====
	class1[1] -> ARPResponder(dpdkdev1)
	-> todev1

	// ===== ARP Replies Process =====
	class1[2] -> [1]arpq_1

check_ip0[1] -> [0]nat_el
	nat_el[0] -> [0]arpq_1
	nat_el[1] -> [0]arpq_0
check_ip0[0] //-> CheckICMPHeader
	-> check_icmp0 :: IPClassifier(dst host dpdkdev0, -)
	-> IPClassifier(icmp type 8)
	-> ICMPPingResponder
	-> [0]arpq_0

	check_icmp0[1] -> ipaddr :: IPAddrRewriter(pattern dpdkdev1 - 0 1, drop)
	ipaddr[0] -> [0]arpq_1
	ipaddr[1] -> [0]arpq_0

check_ip1[1] -> [1]nat_el
check_ip1[0] //-> CheckICMPHeader
	-> check_icmp1 :: IPClassifier(dst host dpdkdev1 and icmp type 8, -)
	-> ICMPPingResponder
	-> [0]arpq_1

	check_icmp1[1] -> [1]ipaddr
