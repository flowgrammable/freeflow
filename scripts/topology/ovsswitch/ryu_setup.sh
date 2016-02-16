#!/usr/bin/env bash

set -e
set -o nounset

echo 'Setting up RYUcontroller'

# Enable RYU controller with openflow 13 application and listen port 6666
/home/vagrant/controllers/ryu/bin/ryu-manager --ofp-tcp-listen-port 6666 --verbose /home/vagrant/controllers/ryu/ryu/app/simple_switch_13.py & 
sleep 5
