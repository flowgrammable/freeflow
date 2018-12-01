#!/bin/bash

if [ $# -ne 3 ]; then
	echo "Usage: $0 input_pcap flow_tuple flow_id"
	exit 1
fi

pcap_in=$1
shift
flowTuple=$1
shift
flowID=$1

ipSrc="$(cut -d',' -f1 <<<$flowTuple)"
ipDst="$(cut -d',' -f2 <<<$flowTuple)"
portSrc="$(cut -d',' -f3 <<<$flowTuple)"
portDst="$(cut -d',' -f4 <<<$flowTuple)"
proto="$(cut -d',' -f5 <<<$flowTuple)"


function filterGZ() {
	local pcap=$1
	shift

	gunzip -c $pcap | tcpdump -r - -w "${pcap}-${flowID}.pcap" "ip proto $proto and host $ipSrc and host $ipDst and port $portSrc and port $portDst"
}

function filter() {
	local pcap=$1
	shift

	tcpdump -r $pcap -w "${pcap}-${flowID}.pcap" "ip proto $proto and host $ipSrc and host $ipDst and port $portSrc and port $portDst"
}

function list() {
	local pcap=$1
	shift

	while read line; do
		dispatch "${pcap_in%/*}/${line}"
	done <"$pcap"
}

function dispatch() {
	local pcap=$1
	shift

	#local file="${pcap_in%/*}/${pcap}"

	if [[ $pcap == *.pcap.gz ]]; then
		filterGZ "$pcap"
	elif [[ $pcap == *.pcap ]]; then
		filter "$pcap"
	elif [[ $pcap == *.files ]]; then
		list "$pcap"
	else
		echo "$pcap is of unknown extension..." 	
	fi
}

dispatch "$pcap_in"

