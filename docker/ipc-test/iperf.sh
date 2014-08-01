#!/bin/sh

# Create base ipc-hook container
. ./build.sh

# Bi-directional iperf test!
TEST_CMD="iperf -c localhost -d"

. ../test.inc

