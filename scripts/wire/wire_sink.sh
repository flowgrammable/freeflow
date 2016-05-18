#!/bin/bash

# Start the sink (netcat)
#netcat localhost -q 15 -d 5000 &>> /dev/null &
#netcat -l -q 15 -d 10.0.0.2 5000 &> /dev/null &
netcat -l -q 15 -d 10.0.0.2 5000 &>> /dev/null
NC_PID=$!
#taskset -c 1 -p $NC_PID
echo "Sink (netcat) started..."

