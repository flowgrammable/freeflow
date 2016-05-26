#!/bin/bash
# Arguments:
#$1 - dest IP address

# Start the source (flowcap)
echo "Starting source ($PWD/flowcap forward)..."
#taskset -c 1 ./flowcap forward $input/smallFlows.pcap 10.0.0.2 5000 200
#./flowcap forward $input/smallFlows.pcap 10.0.0.2 5000 200
./flowcap forward $input/bigFlows.pcap $1 5000 &

