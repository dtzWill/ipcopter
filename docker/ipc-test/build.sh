#!/bin/bash

DIR="$(readlink -f $(dirname $0))"
ROOT="$(readlink -f $DIR/../..)"

LIBIPC_DIR="$ROOT/libipc"
IPCD_DIR="$ROOT/ipcd"

make -C $LIBIPC_DIR clean
make -C $LIBIPC_DIR -j3
cp $LIBIPC_DIR/libipc.so $DIR/

cd $IPCD_DIR
go clean
go build
go test

cp $IPCD_DIR/ipcd $DIR/

cd $DIR
docker build -t wdtz/ipc-hook .
