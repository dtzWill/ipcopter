#!/bin/bash

. ./build.sh

TEST_CMD="
DATA_DIR=/var/lib/lmbench
CONFIG_DIR=\$DATA_DIR/config
CONFIG=\$CONFIG_DIR/CONFIG.\$HOSTNAME
RESULTS_DIR=\$DATA_DIR/results/x86_64-linux-gnu

ln -s /CONFIG /usr/lib/lmbench/bin/x86_64-linux-gnu/CONFIG.\$HOSTNAME
mkdir -p \$CONFIG_DIR
ln -s /CONFIG \$CONFIG
lmbench-run
echo '**************************'
cat /lmbench.log
echo '**************************'
cat \$RESULTS_DIR/\${HOSTNAME}.0
"

. ../test.inc


