#!/bin/sh

# Create base ipc-hook container
. ./build.sh

TAG=will/ipc-hook

TEST_CMD="iperf -s -D; iperf -c localhost"

. ../test.inc

