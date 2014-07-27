# Include for automating testing of applications
# See ipc-test/netperf.sh for example

DISABLE_CMD="rm /etc/ld.so.preload"
# Don't kill ipcd, since bash will have connection to it
# DISABLE_CMD="$DISABLE_CMD && killall ipcd"
DISABLE_CMD="$DISABLE_CMD && rm /bin/ipcd /lib/libipc.so"
DISABLE_CMD="$DISABLE_CMD && rm /tmp/ipcd.sock"

function bench_normal() {
echo
echo
echo "========================================="
echo "Running without our tech:"
echo "-----------------------------------------"
docker run --rm -i $TAG /bin/bash <<EOF
$DISABLE_CMD
$TEST_CMD
EOF
}

function bench_opt() {
echo
echo
echo "========================================="
echo "Running *with* our IPCD tech:"
echo "-----------------------------------------"
docker run --rm -i $TAG /bin/bash <<EOF
$TEST_CMD
grep Optimized /tmp/ipcd.*.log
EOF
}

bench_normal
bench_opt
bench_normal
bench_opt
bench_normal
bench_opt
