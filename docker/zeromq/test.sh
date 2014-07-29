#!/bin/sh

. ./build.sh

TEST_CMD="
cd /zeromq-4.0.4/perf
./local_thr tcp://lo:5555 1 1000000 &
./remote_thr tcp://localhost:5555 1 1000000
sleep 1
wait
./local_thr tcp://lo:5555 10 100000 &
./remote_thr tcp://localhost:5555 10 100000
sleep 1
wait
./local_thr tcp://lo:5555 100 100000 &
./remote_thr tcp://localhost:5555 100 100000
sleep 1
wait
./local_thr tcp://lo:5555 1000 100000 &
./remote_thr tcp://localhost:5555 1000 100000
wait
"

. ../test.inc
