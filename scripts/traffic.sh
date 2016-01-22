(cat /dev/urandom | netcat -u localhost 5001 &); sleep 10; fuser -n udp localhost/5001

