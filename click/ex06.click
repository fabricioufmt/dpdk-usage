// Setting up objects
AddressInfo(
	dpdkdev0	192.168.0.254	192.168.0.0/24	08:00:27:ff:ff:01,
	dpdkdev1	200.200.200.200			08:00:27:ff:ff:02
);

elementclass FromDPDKClassifier {
	$device, $color |
	from :: FromDPDKDevice($device)
		-> Paint($color)
		-> class :: Classifier(12/0800, 12/0806, -)
		class[0] -> [0]output; // IP
		class[1] -> [1]output; // ARP
		class[2] -> [2]output; // others
	ScheduleInfo(from .1);
}
//Scheduling parameters are real numbers that set how often one element should be scheduled in relation to another. For example, if elements A and B have scheduling parameters 2 and 0.5, respectively, then A will be scheduled 2/0.5 = 4 times as often as B. The default scheduling parameter is 1

elementclass TCPUDPClassifier {
	input -> class :: IPClassifier(tcp or udp)
	class[0] -> [0] output
}

check_ip1, check_ip2 :: CheckIPHeader(OFFSET 14, CHECKSUM false)
IPRewriterPatterns(NAT dpdkdev1 1025-65535 - -)
//nat_el :: IPRewriter(pattern NAT 0 1, pass 1) (in this case, sender will receive even there is not a mapping in IPRewriter element)
nat_el :: IPRewriter(pattern NAT 0 1, drop)

// Configuration Click
FromDPDKClassifier(0, 0) 
	// non-IP packets are discarded
	=> check_ip1, Discard, Discard
	
	// non-TCP and non-UDP are discarded
	check_ip1 -> TCPUDPClassifier 

	// considering only packets from intranet (and not to intranet)
	-> IPClassifier(src net dpdkdev0 and not dst net dpdkdev0)
	//-> IPPrint
	-> [0]nat_el[0] 
		-> StoreEtherAddress(dpdkdev1, src) 
		-> StoreEtherAddress(08:00:27:00:00:02, dst)
		//-> IPPrint
		-> ToDPDKDevice(1)
	nat_el[1] 
		-> Strip(14)
		-> EtherEncap(0x0800, dpdkdev0, 08:00:27:00:00:01)
		//-> IPPrint
		-> ToDPDKDevice(0)


FromDPDKClassifier(1, 1) 
	// non-IP packets are discarded
	=> check_ip2, Discard, Discard

	// non-TCP and non-UDP are discarded
	check_ip2 -> TCPUDPClassifier

	// considering only packets to extranet (dst host <dpdkdev1>)
	-> IPClassifier(dst host dpdkdev1)
	//-> IPPrint
	-> [1]nat_el











