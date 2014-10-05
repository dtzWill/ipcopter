#!/bin/bash

# Create base ipc-hook container
. ./build.sh

# Bi-directional iperf test, 5 parallel connections
TEST_CMD="iperf -c localhost -d -P5"

. ../test.inc

