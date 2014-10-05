#!/bin/bash

. ./build.sh

TEST_CMD="
service memcached start
sleep 1
WORKLOAD=basic ./run.sh
service memcached stop
cat results/results.csv
sleep 1"

. ../test.inc


