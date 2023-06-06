#!/bin/bash

MSG="$1"
HOST=10.1.0.3
PORT=8080

printf '\x00\x00\x00\x00\x00\x01\x00\x00%s\r\n' "$MSG" | nc -u $HOST $PORT
