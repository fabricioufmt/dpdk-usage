ControlSocket(TCP, 8012)

out :: ToDPDKDevice(1)
pol :: PolicyCenter
mapping :: MapSession

ipck :: SetIPChecksum
tcpck :: SetTCPChecksum

FromDPDKDevice(0, PROMISC true)
	-> cl0 :: Classifier(12/0800, -)
	-> CheckIPHeader(OFFSET 14)
	-> cl1 :: IPClassifier(syn, -)
	-> haspayload :: HasPayload

cl0[1]
	-> out

haspayload[0]
	-> [0]mapping[0]
	-> ipck
	-> tcpck
	-> out

//from NIC or from Application?
haspayload[1]
	-> pol
	-> [1]mapping[1]
	-> ipck
	-> tcpck
	-> out

cl1[1]
	-> [2]mapping[2]
	-> ipck
	-> tcpck
	-> out

FromDPDKDevice(1, PROMISC true)
	-> ToDPDKDevice(0)
