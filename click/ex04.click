//FromDump(f1a.dump, STOP true)
FromDPDKDevice(0)
  -> ipc1 :: IPClassifier(tcp, udp, -);

  ipc1[0] -> ipc2 :: IPClassifier(src host 149.207.99.128 and src tcp port 80, -)
  ipc1[1] -> Discard
  ipc1[2] -> Discard

  ipc2[0] -> nat_el :: IPRewriter(pattern 1.1.1.1 8080 - - 0 1)

  //ipc2[1] -> ToDump(saida2.dump, ENCAP IP, NANO false)
  //nat_el[0] -> ToDump(saida0.dump, ENCAP IP, NANO false)
  //nat_el[1] -> ToDump(saida1.dump, ENCAP IP, NANO false)

  sched :: SimpleRoundRobinSched
  ipc2[1] -> Queue(100) -> [0]sched
  nat_el[0] -> Queue(100) -> [1]sched
  nat_el[1] -> Queue(100) -> [2]sched

  sched -> ToDump(saidaDPDK.dump, ENCAP IP, NANO false)
  
