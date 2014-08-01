#!/bin/sh

. ./build.sh

TEST_CMD="
service apache2 start
echo 'Keepalive OFF'
ab -n1000 -c1 http://localhost/1M.bin
service apache2 stop"

. ../test.inc


TEST_CMD="
service apache2 start
echo 'Keepalive ON'
ab -k -n1000 -c1 http://localhost/1M.bin
service apache2 stop"

. ../test.inc
