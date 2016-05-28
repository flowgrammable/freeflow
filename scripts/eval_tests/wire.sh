home_dir=$PWD
# Compile the steve app.
echo "Compiling 'lwire' Steve application"
cd ../../../scripts/
./steve_compile wire
app_dir=$PWD/apps/

cd $home_dir
# Driver.
driver=../../build/fp-lite/drivers/wire/fp-wire-select-st

# Flowcap
flowcap_dir=../../build/flowcap
input=input/smallFlows.pcap

echo "Starting 'wire' app"
# Start the freeflow server running the app.
$driver "once" $app_dir &

# Wait for server setup.
sleep 1

echo "Starting 'sink' (flowcap fetch)"
# Start the sink.
$flowcap_dir/flowcap fetch $input 127.0.0.1 5000 &

sleep 1

echo "Starting 'source' (flowcap forward)"
# Start the source.
$flowcap_dir/flowcap forward $input 127.0.0.1 5000 &

wait
