
# Scripting

Do this:

```
for i in {1..100}; do sleep 1; ./flowcap fetch smallFlows.pcap 127.0.0.1 5000 & sleep 1 && ./flowcap forward smallFlows.pcap 127.0.0.1 5000 ; done
```

Broken out:


```
for i in {1..100}; do 
  sleep 1
  ./flowcap fetch smallFlows.pcap 127.0.0.1 5000 & 
  sleep 1 && 
  ./flowcap forward smallFlows.pcap 127.0.0.1 5000
done
```

That is, sleep for a second. Start the fetcher in the background. Sleep to
make sure it starts and connects, and after sleeping run the forwarder.
Tada.

