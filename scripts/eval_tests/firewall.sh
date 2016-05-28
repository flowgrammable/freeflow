home_dir=$PWD
# Compile the steve app.
echo "Compiling 'firewall' Steve application"
cd ../../../scripts/
./steve_compile firewall
app_dir=$PWD/apps/

cd $home_dir
# Driver.
driver=../../build/fp-lite/drivers/firewall/fp-firewall

# Flowcap
flowcap_dir=../../build/flowcap
input=input/smallFlows.pcap

echo "Starting 'firewall' app"
# Start the freeflow server running the app.
$driver "once" $app_dir &

# Wait for server setup.
sleep 1

# Start the sinks.
echo "Starting 'sink 1' (netcat)"
netcat localhost -q 15 -d 5000 &>> /dev/null &
echo "Starting 'sink 2' (netcat)"
netcat localhost -q 15 -d 5000 &>> /dev/null &

sleep 1

echo "Starting 'source' (flowcap forward)"
# Start the source.
$flowcap_dir/flowcap forward $input 127.0.0.1 5000 &

wait
