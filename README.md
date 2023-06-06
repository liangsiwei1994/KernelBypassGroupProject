# What this does

Here, we run a DPDK middlebox which accepts UDP packets (```memcached``` get requests) from clients and merges them into a single packet. It then sends this combined packet to a ```memcached``` server. The server answers the requests, and the middlebox steers the replies to the correct clients.

# Why this is important

Extracting a packet's payload using the kernel's network stack involves several system calls and context switches. By reducing the number of packets sent to ```memcached``` without reducing the number of requests, we reduce the time taken for ```memcached``` to respond. We further optimise this by removing duplicate requests across clients; the associated ```memcached``` reply is simply copied and sent to clients making these requests.

# How to run our project

It is assumed that you have started ```memcached``` and that it is listening on 10.1.0.3 (```-l 10.1.0.3``` on ```/etc/memcached.conf```) for UDP packets on port 8080 (```-U 8080```). Continue to listen on TCP port 11211 (```-p 11211```) to permit manual value setting via ```telnet```. Set your values manually using ```telnet``` before you begin.

Begin at the project directory.

Allocate enough pages by calling ```./scripts/hugepages.sh```. 

Then, run ```. ./scripts/startmiddlebox.sh```, which will build and run the middlebox.

In another terminal window, run ```sudo ./scripts/testbed-setup.sh``` from the project directory.

In a third terminal window, edit ```./scripts/create-client-1.sh``` to ```get``` any number of the specified keys, and run it. This will create our first client.

In a fourth terminal window, edit ```./scripts/create-client-2.sh```, to ```get``` any number of the specified keys, and run it. This will create our second client.

We expect each client to display the requested values.

# Acknowledgements

This is a collaborative effort between Ethan Sim, Christopher Adnel, Alvin Jonathan, Brendan Cheu and Liang Si Wei which is supervised by Dr. Marios Kogias of Imperial College London (MSc Computing 2022-2023).
