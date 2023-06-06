#!/bin/bash

cd middlebox
make clean
make
cd ..
sudo ./middlebox/build/middlebox -l 0 --vdev=net_tap0,iface=tapdpdk,mac=fixed
