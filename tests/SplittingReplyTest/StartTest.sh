#!/bin/bash

# Setting up hugepages -----------------------------------------------------------------------------------------------------

sudo sh -c 'for i in /sys/devices/system/node/node*/hugepages/hugepages-2048kB/nr_hugepages; do echo 512 > $i; done'
sudo mkdir -p /mnt/huge
sudo mount -t hugetlbfs nodev /mnt/huge

# Make and running middlebox -----------------------------------------------------------------------------------------------

cd middlebox
make clean
make
cd ..
(sudo ./middlebox/build/middlebox -l 0 --vdev=net_tap0,iface=tapdpdk,mac=fixed > mbox_log.txt 2> mbox_log.txt &)
sleep 1

# Setting up the bridge ----------------------------------------------------------------------------------------------------

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

# Setting up memcached -----------------------------------------------------------------------------------------------------

# remember to setup memcached conf file to run at this address and port!!
HOST='10.1.0.3 11211'

sudo service memcached restart
sleep 1
(
echo open "$HOST"
sleep 0.1

# adding first key-value pair
echo "set key1 0 3600 6"
sleep 0.1
echo "value1"
sleep 0.1

# adding first key-value pair
echo "set key2 0 3600 6"
sleep 0.1
echo "value2"
sleep 0.1

# adding first key-value pair
echo "set key3 0 3600 6"
sleep 0.1
echo "value3"
sleep 0.1

# adding first key-value pair
echo "set key4 0 3600 6"
sleep 0.1
echo "value4"
sleep 0.1

echo "quit"
) | telnet

# Update routing table -----------------------------------------------------------------------------------------------------

(sudo ip route add 10.1.0.2 via 10.1.0.3 dev veth2 > routing_log.txt 2> routing_log.txt &)

# Making and running client ----------------------------------------------------------------------------------------------------

cd client
printf '\r'
(make; ./client veth0 10.1.0.1 key1 value1 key2 value2 > client_1_log.txt 2> client_1_log.txt &)
./client veth4 10.1.0.4 key3 value3 key4 value4
