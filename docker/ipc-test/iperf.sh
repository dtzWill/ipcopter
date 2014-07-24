#!/bin/sh

# Create base ipc-hook container
. ./build.sh

TEST_CMD="iperf -s -D; iperf -c localhost"

. ../test.inc

