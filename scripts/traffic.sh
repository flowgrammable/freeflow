#cat /dev/urandom | netcat -u localhost 5001
dd if=/dev/zero 2>/dev/null | openssl enc -aes-256-ctr -pass pass:"$(dd if=/dev/urandom bs=128 count=1 2>/dev/null | base64)" -nosalt | netcat -u localhost 5001
