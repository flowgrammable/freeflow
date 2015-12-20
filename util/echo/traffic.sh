while read line; do echo "$line"; done < out.pcap | netcat -t localhost 5001
