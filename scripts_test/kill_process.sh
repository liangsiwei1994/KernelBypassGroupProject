#!/bin/bash

# kill all server processes
if pgrep -x "server" > /dev/null
then
    sudo killall -9 server
fi


# kill all middlebox processes
if pgrep -x "middlebox" > /dev/null
then
    sudo killall -9 middlebox
fi

