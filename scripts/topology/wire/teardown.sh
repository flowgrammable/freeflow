#!/usr/bin/env bash

set -e
set -o nounset

echo 'Tearing down Wire'

# Delete the two namespaces
ip netns del host0
ip netns del host1

