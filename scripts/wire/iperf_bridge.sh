#!/bin/bash

# setup sink
ip netns exec host0 iperf -s -m &

#setup source
#ip netns exec host1 iperf -c 10.0.0.1 -P 1 -m 
ip netns exec host1 iperf -c 10.0.0.1 -P 1 -M 700 -t 60 -m 

