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

cd ../flowcap

# Start the sink (netcat)
#netcat localhost -q 15 -d 5000 &>> /dev/null &
#netcat -l -q 15 -d 10.0.0.2 5000 &> /dev/null &
#command='netcat -l -q 15 -d 10.0.0.2 5000 &>> /dev/null'
$bin/wire_sink.sh &
NC_PID=$!
echo "Sink (netcat) started..."

# Pause for a second.
sleep 1

# Start the source (flowcap)
time $bin/wire_source.sh 127.0.0.1

# Close the sink
sleep 2
echo "Closing sink (netcat)..."
kill -9 $NC_PID

#$bin/teardown_bridge.sh

#cat $output

