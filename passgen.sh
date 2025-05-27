#!/bin/bash

echo "Password generator 2000"
echo -n "Enter password length: "
read LENGTH

SEED=$(openssl rand 1000 | base64 -w 0 )
SEED_LENGTH=${#SEED}
let RANGE=$SEED_LENGTH-$LENGTH
let START_POS=$RANDOM*$RANGE/32767
let END_POS=START_POS+LENGTH-1

echo $SEED|cut -c$START_POS-$END_POS
