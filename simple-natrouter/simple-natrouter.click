// Setting up objects
AddressInfo(
	dpdkdev0	10.0.1.254	10.0.1.0/24	08:00:27:ff:ff:01,
	dpdkdev1	10.0.2.254	10.0.2.0/24	08:00:27:ff:ff:02,
	extern_ip	20.20.20.20			
);

ControlSocket(TCP, 8012)

rt :: LinearIPLookup(
	10.0.1.0/24			0,
	10.0.2.0/24			1,
	10.0.3.0/24 	10.0.1.1 	0,
	10.0.4.0/24 	10.0.1.2 	0,
	10.0.5.0/24	10.0.2.1	1,
	10.0.6.0/24	10.0.2.2	1,
	0/0 				2,
	10.0.1.254/32			3,
	10.0.2.254/32			3)

todev0 :: ToDPDKDevice(0)
todev1 :: ToDPDKDevice(1)

arpq_0 :: ARPQuerier(dpdkdev0) -> todev0
arpq_1 :: ARPQuerier(dpdkdev1) -> todev1

ipr0 :: IPRewriter(pattern extern_ip 1025-65535 - - 0 1, drop, DST_ANNO false)
ipaddr0 :: IPAddrRewriter(pattern extern_ip - 0 1, drop)

FromDPDKDevice(0)
	// IP, ARP Requests, ARP Replies
	-> class0 :: Classifier(12/0800, 12/0806 20/0001, 12/0806 20/0002)

	// ===== IP Process =====
	-> Strip(14)
	-> CheckIPHeader(CHECKSUM false)
	-> GetIPAddress(16) -> [0]rt

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
	-> ipclass1 :: IPClassifier(tcp or udp, icmp)
	ipclass1[0] -> [1]ipr0
	ipclass1[1] -> [1]ipaddr0

	ipr0[1] -> GetIPAddress(16) -> [0]rt
	ipaddr0[1] -> GetIPAddress(16) -> [0]rt

	// ===== ARP Requests Process =====
	class1[1] -> ARPResponder(dpdkdev1)
	-> todev1

	// ===== ARP Replies Process =====
	class1[2] -> [1]arpq_1


rt[0] -> [0]arpq_0
rt[1] -> ipclass0 :: IPClassifier(tcp or udp, icmp)
ipclass0[0] -> [0]ipr0 -> [0]arpq_1
ipclass0[1] -> [0]ipaddr0 -> [0]arpq_1
rt[2] -> Discard

//to middlebox
rt[3] -> IPClassifier(icmp and icmp type 8)
	-> ICMPPingResponder
	-> [0]rt
