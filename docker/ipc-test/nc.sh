#!/bin/sh

# Create base ipc-hook container
. ./build.sh

# Send 5GB over nc, track transfer speed.
TEST_CMD="nc -dlp 3000 |pv -f > /dev/null &
  sleep 1
  dd if=/dev/zero bs=1M count=5120 status=none|nc localhost 3000"

. ../test.inc

