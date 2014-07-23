# Abort script if anything bad happens
# http://redsymbol.net/articles/unofficial-bash-strict-mode/
set -euo pipefail
IFS=$'\n\t'

DIR="$(readlink -f $(dirname $0))"
ROOT="$(readlink -f $DIR/../..)"


# Path of libipc project
LIBIPC_DIR="$ROOT/libipc"
# Path of ipcd project
IPCD_DIR="$ROOT/ipcd"
# Where to temporarily store the built artifacts
BIN_DIR="$DIR/tmp"
mkdir -p $BIN_DIR


make -C $LIBIPC_DIR clean
make -C $LIBIPC_DIR -j3
cp $LIBIPC_DIR/libipc.so $BIN_DIR/

cd $IPCD_DIR
go clean
go build
go test

cp $IPCD_DIR/ipcd $BIN_DIR/

cd $DIR
TAG=$USER/ipc-$NAME
docker build -t $TAG .

rm -rf $BIN_DIR

echo ""
echo "Successfully created ipcopter container, tagged it as '$TAG'"
echo "To start a shell in this container, run the following command:"
echo "docker run -i -t $TAG"
