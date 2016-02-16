#!/usr/bin/env bash

set -e
set -o nounset

echo 'Setting up UUT Framework'

# Create 2 guest containers
ip netns add host0
ip netns add host1

# Create an ethernet wire and link the containers
ip link add veth0 type veth peer name veth1
ip link add veth2 type veth peer name veth3
ip link set veth0 netns host0
ip link set veth2 netns host1

# Add an IP address for each end of the wire
ip netns exec host0 ip addr add 10.0.0.1/24 dev veth0
ip netns exec host1 ip addr add 10.0.0.2/24 dev veth2

# Create a host bridge and add the virtual interfaces
#brctl addbr br0
#brctl addif br0 veth1
#brctl addif br0 veth3

# Enable host interfaces
ip link set veth1 up
ip link set veth3 up
#ip link set br0 up

# Enable the guest container interfaces
ip netns exec host0 ip link set veth0 up
ip netns exec host1 ip link set veth2 up
