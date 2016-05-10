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
#output=$PWD/wire_veth.txt
#drivers/wire/fp-wire-epoll-sta &> $output &
#WIRE_PID=$!
# Create 2 guest containers
ip netns add host0
ip netns add host1

# Create an ethernet wire and link the containers
ip link add veth0 type veth peer name veth1
ip link add veth2 type veth peer name veth3
ip link set veth0 netns host0
ip link set veth2 netns host1

# Add an IP address for each end of the wire
ip netns exec host0 ip addr add 10.0.0.1/24 dev veth0
ip netns exec host1 ip addr add 10.0.0.2/24 dev veth2

# Disable Segmentation & Checksum Offload
#ip netns exec host0 ethtool -K veth0 tso off ufo off gso off tx off rx off
#ip netns exec host1 ethtool -K veth2 tso off ufo off gso off tx off rx off
#ethtool -K veth1 tso off ufo off gso off tx off rx off
#ethtool -K veth3 tso off ufo off gso off tx off rx off

# Create a host bridge and add the virtual interfaces
brctl addbr br0
brctl addif br0 veth1
brctl addif br0 veth3

# Enable the guest container interfaces
ip netns exec host0 ip link set veth0 up
ip netns exec host1 ip link set veth2 up
ip link set veth1 up
ip link set veth3 up
ip link set br0 up
echo "veth bridge started..."

# Pause for a second.
sleep 1

cd ../flowcap

# Start the sink (netcat)
#netcat localhost -q 15 -d 5000 &>> /dev/null &
#netcat -l -q 15 -d 10.0.0.2 5000 &> /dev/null &
command='netcat -l -q 15 -d 10.0.0.2 5000 &>> /dev/null'
ip netns exec host1 $command &
#NC_PID=$!
echo "Sink (netcat) started..."

# Pause for a second.
sleep 1

# Start the source (flowcap)
echo "Starting $PWD/flowcap..."
time ip netns exec host0 ./flowcap forward $input/smallFlows.pcap 10.0.0.2 5000 200

# Close the sink
sleep 2
#echo "Closing sink (netcat)..."
#kill -9 $NC_PID

echo "Tearing down veth..."
#kill -2 $WIRE_PID
# Delete the two guest namespaces
ip link set br0 down
brctl delbr br0
ip netns del host0
ip netns del host1

#cat $output

