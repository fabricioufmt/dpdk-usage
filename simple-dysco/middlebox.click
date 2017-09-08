// Setting up objects
AddressInfo(
	dpdkdev0	10.0.1.254	10.0.1.0/24	08:00:27:ff:ff:01,
	dpdkdev1	10.0.2.254	10.0.2.0/24	08:00:27:ff:ff:02,
);

ControlSocket(TCP, 8012)

rt :: LinearIPLookup(
	10.0.1.0/24			0,
	10.0.2.0/24			1,
	0/0 				2,
	10.0.1.254/32			3,
	10.0.2.254/32			3)

todev0 :: ToDPDKDevice(0)
todev1 :: ToDPDKDevice(1)

arpq_0 :: ARPQuerier(dpdkdev0) -> todev0
arpq_1 :: ARPQuerier(dpdkdev1) -> todev1

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
	-> GetIPAddress(16) -> [0]rt

	// ===== ARP Requests Process =====
	class1[1] -> ARPResponder(dpdkdev1)
	-> todev1

	// ===== ARP Replies Process =====
	class1[2] -> [1]arpq_1


rt[0] -> [0]arpq_0
rt[1] -> ipclass0 :: IPClassifier(syn, -)
ipclass0[0] -> dysco :: DyscoClassifier
ipclass0[1] -> [0]arpq_1

dysco[0] -> [0]arpq_1
dysco[1] -> [0]arpq_1

rt[2] -> Discard

//to middlebox
rt[3] -> IPClassifier(icmp and icmp type 8)
	-> ICMPPingResponder
	-> [0]rt

