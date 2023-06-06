#!/bin/bash

input="config-files/key-value-set.csv"

HOST='10.1.0.3 11211'

service memcached restart
sleep 1
(
echo open "$HOST"
sleep 0.1

while IFS= read -r line
do
	echo "$line"
	sleep 0.1
done < "$input"


echo "quit"
) | telnet

echo 'telnet done'

