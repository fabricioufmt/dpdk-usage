# Pausa o servico de rede
stop network-manager

# Cria os network namesspaces
ip netns add c3
ip netns add c4
ip netns add r3
ip netns add r4

# Cria os links e as interfaces virtuais
ip link add c3-0 type veth peer name c3-1
ip link add c4-0 type veth peer name c4-1

# Move as interfaces para os namespaces
ip link set c3-0 netns c3
ip link set c4-0 netns c4
ip link set c3-1 netns r3
ip link set c4-1 netns r4
ip link set eth0 netns r3
ip link set eth1 netns r4

# Configura os endereços IPs e ativa as interfaces de rede
ip netns exec c3 ifconfig c3-0 10.0.5.2 netmask 255.255.255.0 up
ip netns exec r3 ifconfig c3-1 10.0.5.1 netmask 255.255.255.0 up
ip netns exec c4 ifconfig c4-0 10.0.6.2 netmask 255.255.255.0 up
ip netns exec r4 ifconfig c4-1 10.0.6.1 netmask 255.255.255.0 up
ip netns exec r3 ifconfig eth0 10.0.2.1 netmask 255.255.255.0 up
ip netns exec r4 ifconfig eth1 10.0.2.2 netmask 255.255.255.0 up
ip netns exec c3 ifconfig lo up
ip netns exec c4 ifconfig lo up
ip netns exec r3 ifconfig lo up
ip netns exec r4 ifconfig lo up

# Adiciona rotas estáticas
ip netns exec c3 route add -net 0.0.0.0 gw 10.0.5.1
ip netns exec c4 route add -net 0.0.0.0 gw 10.0.6.1
ip netns exec r3 route add -net 0.0.0.0 gw 10.0.2.254
ip netns exec r4 route add -net 0.0.0.0 gw 10.0.2.254

# Ativa roteamento 
ip netns exec c3 sh -c 'echo 1 > /proc/sys/net/ipv4/ip_forward'
ip netns exec c4 sh -c 'echo 1 > /proc/sys/net/ipv4/ip_forward'
ip netns exec r3 sh -c 'echo 1 > /proc/sys/net/ipv4/ip_forward'
ip netns exec r4 sh -c 'echo 1 > /proc/sys/net/ipv4/ip_forward'
echo 1 > /proc/sys/net/ipv4/ip_forward
