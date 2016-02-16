#!/usr/bin/env bash

set -e
set -o nounset

echo 'Tearing down Bridge'
kill `ps aux | grep netmapbin/bridge | grep -v grep | awk '{print $2}'`

# Delete the two guest namespaces
ip netns del host0
ip netns del host1

