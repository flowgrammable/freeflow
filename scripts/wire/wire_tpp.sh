# Set input file.
input=$PWD/input

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
output=$PWD/wire_tpp.txt
drivers/wire/fp-wire-epoll-tpp &> $output &
WIRE_PID=$!
echo "Wire-TPP started... writing to $output"

# Pause for a second.
sleep 1

cd ../flowcap

# Start the sink (netcat)
netcat localhost 5000 &>> /dev/null &
NC_PID=$!
echo "Sink (netcat) started..."

# Pause for a second.
sleep 1

# Start the source (flowcap)
echo "Starting $PWD/flowcap..."
./flowcap forward $input/smallFlows.pcap 127.0.0.1 5000 100

sleep 2
echo "Closing sink (netcat)..."
kill -9 $NC_PID

echo "Stopping Wire-STA..."
kill -2 $WIRE_PID