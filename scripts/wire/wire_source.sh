#!/bin/bash

# Start the source (flowcap)
echo "Starting $PWD/flowcap..."
taskset -c 1 ./flowcap forward $input/smallFlows.pcap 10.0.0.2 5000 200

