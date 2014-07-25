#!/bin/sh

echo "This test seems to be unreliable, presently, when automated."
echo "Not sure why, will investigate, but it's a known issue :)."
echo "Press control-c to abort now, or any regular key to proceed anyway"
read

# Create base ipc-hook container
. ./build.sh

TEST_CMD="iperf -s -D; iperf -c localhost"

. ../test.inc

