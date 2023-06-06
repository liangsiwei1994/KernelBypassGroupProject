#!/bin/bash

set -xe

bridge=br0

inf0=veth0
inf1=veth1

inf2=veth2
inf3=veth3

inf4=veth4
inf5=veth5

inf0ip="10.1.0.1"

inf2ip="10.1.0.3"

inf4ip="10.1.0.4"

mac0="DE:AD:BE:EF:7B:15"

mac2="DE:AD:BE:EF:8B:16"

mac4="DE:AD:BE:EF:9B:17"

infdpdk=tapdpdk

### Create a bridge
if ! ip link show $bridge &> /dev/null; then
	brctl addbr $bridge
	ip link set dev $bridge up
fi

### Create a veth pair
### Add the one end on the bridge and give an IP address to the other end
if ! ip link show $inf0 &> /dev/null; then
	ip link add name $inf0 type veth peer name $inf1
	ip addr add $inf0ip brd + dev $inf0
	ip link set $inf0 address $mac0
	brctl addif $bridge $inf1
	ip link set dev $inf0 up
	ip link set dev $inf1 up
fi

### Add server
if ! ip link show $inf2 &> /dev/null; then
	ip link add name $inf2 type veth peer name $inf3
	ip addr add $inf2ip brd + dev $inf2
	ip link set $inf2 address $mac2
	brctl addif $bridge $inf3
	ip link set dev $inf2 up
	ip link set dev $inf3 up
fi

### Add client 2
if ! ip link show $inf4 &> /dev/null; then
	ip link add name $inf4 type veth peer name $inf5
	ip addr add $inf4ip brd + dev $inf4
	ip link set $inf4 address $mac4
	brctl addif $bridge $inf5
	ip link set dev $inf4 up
	ip link set dev $inf5 up
fi

### Attach dpdk tap to the bridge
brctl addif $bridge $infdpdk
