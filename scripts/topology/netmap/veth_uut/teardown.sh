#!/usr/bin/env bash

set -e
set -o nounset

echo 'Tearing down veth unit-under-test framework'

# Delete the two guest namespaces
ip netns del host0
ip netns del host1

