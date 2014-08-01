#!/bin/sh

PORTS=`seq 3000 3010`

echo "Starting 'servers'..."
for p in $PORTS; do
	dd if=/dev/zero bs=1M count=1000 status=none|nc -dlp $p|pv -f > /dev/null 2> nc.server.$p.log &
done
sleep 1
echo "Starting 'clients'..."
for p in $PORTS; do
	dd if=/dev/zero bs=1M count=1000 status=none|nc localhost $p|pv -f > /dev/null 2> nc.client.$p.log &
done
for p in $PORTS; do
	wait
done
for p in $PORTS; do
	wait
done
echo
echo "==================================="
echo "Dumping logs:"
for p in $PORTS; do
	echo "-- Server log $p ---------------"
	cat nc.server.$p.log
	echo "-- Client log $p ---------------"
	cat nc.client.$p.log
done
