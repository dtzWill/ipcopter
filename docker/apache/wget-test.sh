#!/bin/bash

. ./build.sh

TEST_CMD="
service apache2 start
wget http://localhost/100M.bin -O /dev/null
wget http://localhost/100M.bin -O /dev/null
wget http://localhost/100M.bin -O /dev/null
wget http://localhost/10M.bin -O /dev/null
wget http://localhost/10M.bin -O /dev/null
wget http://localhost/10M.bin -O /dev/null
wget http://localhost/1M.bin -O /dev/null
wget http://localhost/1M.bin -O /dev/null
wget http://localhost/1M.bin -O /dev/null
service apache2 stop"

. ../test.inc


