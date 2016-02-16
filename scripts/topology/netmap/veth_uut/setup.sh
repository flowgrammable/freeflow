#!/usr/bin/env bash

set -e
set -o nounset

echo 'Setting up Veth unit-under-test Framework'

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

# Disable Segmentation & Checksum Offload
ip netns exec host0 ethtool -K veth0 tso off ufo off gso off tx off rx off
ip netns exec host1 ethtool -K veth2 tso off ufo off gso off tx off rx off
ethtool -K veth1 tso off ufo off gso off tx off rx off
ethtool -K veth3 tso off ufo off gso off tx off rx off

# Connect netmap veth end to virtual host's stack
#ip netns exec host0 '/home/lam89/netmap_orig.git/examples/bridge -i netmap:veth0'
#ip netns exec host1 '/home/lam89/netmap_orig.git/examples/bridge -i netmap:veth2'

# Launch the unit-under-test (i.e. bridge, noproto, etc:)
#/home/vagrant/netmapbin/bridge -i netmap:veth1 -i netmap:veth3 &

# Enable host interfaces
ip link set veth1 up
ip link set veth3 up

# Enable the guest container interfaces
ip netns exec host0 ip link set veth0 up
ip netns exec host1 ip link set veth2 up

