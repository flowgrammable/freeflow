while read line; do echo "$line"; done < out.pcap | netcat localhost 5001

