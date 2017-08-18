# Pausa o servico de rede
stop network-manager

# Cria os network namespaces
ip netns add c1
ip netns add c2
ip netns add r1
ip netns add r2

# Cria os links e as interfaces virtuais
ip link add c1-0 type veth peer name c1-1
ip link add c2-0 type veth peer name c2-1

# Move as interfaces para os namespaces
ip link set c1-0 netns c1
ip link set c2-0 netns c2
ip link set c1-1 netns r1
ip link set c2-1 netns r2
ip link set eth0 netns r1
ip link set eth1 netns r2

# Configura os endereços IPs e ativa as interfaces de rede
ip netns exec c1 ifconfig c1-0 10.0.3.2 netmask 255.255.255.0 up
ip netns exec r1 ifconfig c1-1 10.0.3.1 netmask 255.255.255.0 up
ip netns exec c2 ifconfig c2-0 10.0.4.2 netmask 255.255.255.0 up
ip netns exec r2 ifconfig c2-1 10.0.4.1 netmask 255.255.255.0 up
ip netns exec r1 ifconfig eth0 10.0.1.1 netmask 255.255.255.0 up
ip netns exec r2 ifconfig eth1 10.0.1.2 netmask 255.255.255.0 up
ip netns exec r1 ifconfig lo up
ip netns exec r2 ifconfig lo up
ip netns exec c1 ifconfig lo up
ip netns exec c2 ifconfig lo up

# Adiciona rotas estáticas
ip netns exec c1 route add -net 0.0.0.0 gw 10.0.3.1
ip netns exec c2 route add -net 0.0.0.0 gw 10.0.4.1
ip netns exec r1 route add -net 0.0.0.0 gw 10.0.1.254
ip netns exec r2 route add -net 0.0.0.0 gw 10.0.1.254

# Ativa roteamento 
ip netns exec c1 sh -c 'echo 1 > /proc/sys/net/ipv4/ip_forward'
ip netns exec c2 sh -c 'echo 1 > /proc/sys/net/ipv4/ip_forward'
ip netns exec r1 sh -c 'echo 1 > /proc/sys/net/ipv4/ip_forward'
ip netns exec r2 sh -c 'echo 1 > /proc/sys/net/ipv4/ip_forward'
echo 1 > /proc/sys/net/ipv4/ip_forward
