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

# Start the bridge application.
$bin/setup_bridge.sh

# Pause for a second.
sleep 1

cd ../flowcap

# Start the sink (netcat)
#netcat localhost -q 15 -d 5000 &>> /dev/null &
#netcat -l -q 15 -d 10.0.0.2 5000 &> /dev/null &
#command='netcat -l -q 15 -d 10.0.0.2 5000 &>> /dev/null'
ip netns exec host1 $bin/wire_sink.sh 10.0.0.2 &
#NC_PID=$!
echo "Sink (netcat) started..."

# Pause for a second.
sleep 1

# Start the source (flowcap)
time ip netns exec host0 $bin/wire_source.sh 10.0.0.2

# Close the sink
sleep 2
#echo "Closing sink (netcat)..."
#kill -9 $NC_PID

$bin/teardown_bridge.sh

#cat $output

