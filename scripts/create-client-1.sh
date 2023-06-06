#!/bin/bash

cd client
make
cd ..
./client/client veth0 10.1.0.1 "gets vowel power lived close"
