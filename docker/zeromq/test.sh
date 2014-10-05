#!/bin/bash

. ./build.sh

TEST_CMD="
cd /zeromq-4.0.4/perf
./local_thr tcp://lo:5555 1 10000000 &
./remote_thr tcp://localhost:5555 1 10000000
sleep 1
wait
./local_thr tcp://lo:5555 10 1000000 &
./remote_thr tcp://localhost:5555 10 1000000
sleep 1
wait
./local_thr tcp://lo:5555 100 100000 &
./remote_thr tcp://localhost:5555 100 100000
sleep 1
wait
./local_thr tcp://lo:5555 1000 10000 &
./remote_thr tcp://localhost:5555 1000 10000
wait
sleep 1
./local_thr tcp://lo:5555 10000 1000 &
./remote_thr tcp://localhost:5555 10000 1000
wait
sleep 1
./local_thr tcp://lo:5555 100000 100 &
./remote_thr tcp://localhost:5555 100000 100
wait
"

. ../test.inc
