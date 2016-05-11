#!/bin/bash

# setup sink
ip netns exec host0 iperf -s &

#setup source
ip netns exec host1 iperf -c 10.0.0.1 -P 1 

