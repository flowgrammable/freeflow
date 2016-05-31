#!/bin/bash

mkdir -p tmp
mount -o size=20G -t tmpfs none tmp
mergecap -a -w tmp/biggerFlows.pcap `for i in {1..50}; do echo 'bigFlows.pcap'; done`

