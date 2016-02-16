#!/usr/bin/env bash

set -e
set -o nounset

echo 'Setting up OVSswitch'

# Create 2 guest containers
ip netns add host0
ip netns add host1
#ip netns add host2
# Create an ethernet wire and link the containers
ip link add veth0 type veth peer name veth1
ip link add veth2 type veth peer name veth3
#ip link add veth4 type veth peer name veth5
ip link set veth0 netns host0
ip link set veth2 netns host1
#ip link set veth4 netns host2

# Add an IP address for each end of the wire
ip netns exec host0 ip addr add 10.0.0.1/24 dev veth0
ip netns exec host1 ip addr add 10.0.0.2/24 dev veth2
#ip netns exec host2 ip addr add 10.0.0.3/24 dev veth4

# Create a host bridge and add the virtual interfaces
ovs-vsctl add-br br0
ovs-vsctl add-port br0 veth1
ovs-vsctl add-port br0 veth3
#ovs-vsctl add-port br0 veth5
ovs-vsctl set-controller br0 tcp:127.0.0.1:6666
ovs-vsctl set Bridge br0 protocols=OpenFlow13


# Enable host interfaces
ip link set veth1 up
ip link set veth3 up
#ip link set veth5 up
ip link set br0 up

# Enable the guest container interfaces
ip netns exec host0 ip link set veth0 up
ip netns exec host1 ip link set veth2 up
#ip netns exec host2 ip link set veth4 up
