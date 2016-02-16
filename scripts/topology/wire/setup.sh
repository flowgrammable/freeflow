#!/usr/bin/env bash

set -e
set -o nounset

echo 'Setting up Wire'

# Create 2 guest containers
ip netns add host0
ip netns add host1

# Create an ethernet wire and link the containers
ip link add veth0 type veth peer name veth1
ip link set veth0 netns host0
ip link set veth1 netns host1

# Add an IP address for each end of the wire
ip netns exec host0 ip addr add 10.0.0.1/24 dev veth0
ip netns exec host1 ip addr add 10.0.0.2/24 dev veth1

ip netns exec host0 ip link set veth0 up
ip netns exec host1 ip link set veth1 up

