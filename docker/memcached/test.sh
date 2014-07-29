#!/bin/sh

. ./build.sh

TEST_CMD="
service memcached start
sleep 1
cd memcachetest
./memcachetest -h 127.0.0.1 -t 1 -P 33 -i 10000 -c 200000 -l -T 10
service memcached stop
sleep 1"

. ../test.inc


