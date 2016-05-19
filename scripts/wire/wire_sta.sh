#!/bin/bash

# Set script path.
export bin=$PWD

# Set input file.
export input=$PWD/input

# Move to build folder.
cd ../../build
if [ $? -ne 0 ];
	then
	echo "Error. No build folder found"
	exit 1
fi

# Navigate to the fp-lite folder.
#
# If there is no release folder, it shouldn't matter.
cd release
cd fp-lite

# Start the wire application.
output=$PWD/wire_sta.txt
#taskset -c 2 drivers/wire/fp-wire-epoll-sta &> $output &
drivers/wire/fp-wire-epoll-sta &> $output &
WIRE_PID=$!
echo "Wire-STA started... writing to $output"

# Pause for a second.
sleep 1

cd ../flowcap

# Start the sink (netcat)
#taskset -c 1 netcat localhost -q 15 -d 5000 &>> /dev/null &
netcat localhost -q 15 -d 5000 &>> /dev/null &
NC_PID=$!
echo "Sink (netcat) started..."

# Pause for a second.
sleep 1

# Start the source (flowcap)
#echo "Starting $PWD/flowcap..."
#time taskset -c 1 ./flowcap forward $input/smallFlows.pcap 127.0.0.1 5000 200
#time ./flowcap forward $input/smallFlows.pcap 127.0.0.1 5000 200
#time ./flowcap forward $input/bigFlows.pcap 127.0.0.1 5000 25
time $bin/wire_source.sh 127.0.0.1

# Close the sink
sleep 2
echo "Closing sink (netcat)..."
kill -9 $NC_PID

echo "Stopping Wire-STA..."
kill -2 $WIRE_PID

#cat $output

