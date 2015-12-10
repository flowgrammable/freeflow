while read line; do echo "$line"; done < out.pcap | netcat -u localhost 5001
