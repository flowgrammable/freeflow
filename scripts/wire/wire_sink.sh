#!/bin/bash
# Arguments
#$1 - src IP address

# Start the sink (netcat)
#netcat localhost -q 15 -d 5000 &>> /dev/null &
#netcat -l -q 15 -d 10.0.0.2 5000 &> /dev/null &
#netcat -l -q 15 -d $1 5000 &>> /dev/null
#NC_PID=$!
#taskset -c 1 -p $NC_PID
#echo "Sink (netcat) started..."
echo "Starting sink ($PWD/flowcap $1)..."
./flowcap $1 $input/bigFlows.pcap $2 5000 &
