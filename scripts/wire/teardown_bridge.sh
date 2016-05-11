#!/bin/bash

echo "Tearing down veth..."
# Delete the two guest namespaces
ip link set br0 down
brctl delbr br0
ip netns del host0
ip netns del host1

