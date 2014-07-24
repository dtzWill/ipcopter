#!/bin/sh

# Create base ipc-hook container
. ./build.sh

TEST_CMD="netserver; netperf -t TCP_STREAM"

. ../test.inc

