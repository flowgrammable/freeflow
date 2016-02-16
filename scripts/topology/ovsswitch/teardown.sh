#!/usr/bin/env bash

set -e
set -o nounset

echo 'Tearing down Bridge'

# Delete the two guest namespaces
ip netns del host0
ip netns del host1
# ip link del veth1
# ip link del veth3


# Delete the central bridge
ip link set br0 down
ovs-vsctl del-br br0

