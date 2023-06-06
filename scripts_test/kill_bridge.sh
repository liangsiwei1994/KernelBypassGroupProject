#!/bin/bash

if ifconfig br0 ; then
    echo "hello"
    sudo ip link delete br0 type bridge
fi

if ifconfig veth0 ; then
    sudo ip link delete veth0 type bridge
fi

if ifconfig veth2 ; then
    sudo ip link delete veth2 type bridge
fi

if ifconfig veth4 ; then
    sudo ip link delete veth4 type bridge
fi






