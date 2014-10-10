#!/bin/bash

# We're short on time, forgive me.

function status() {
  echo
  echo
  echo "--------------------------------------------------------------------------------"
  echo "$@"
  echo "--------------------------------------------------------------------------------"
}

function update() {
  echo "  * $@"
}

# Just assume ipc-hook base image already exists for now.
echo "(Assuming ipc-hook base image already exists!)"
# status "Building components and base image"
# cd ../ipc-test/ && ./build.sh && cd -


status "Grabbing netpipe source and patch"

SCRIPTS_DIR=$HOME/ipcopter-scripts
NETPIPE_SRC=$SCRIPTS_DIR/netpipe/NetPIPE-3.7.1.tar.gz
NETPIPE_PATCH=$SCRIPTS_DIR/netpipe/fix_port_flag.patch

cp $NETPIPE_SRC $NETPIPE_PATCH netpipe/

status "Generating containers"

update "Configuring local Dockerfiles..."
for f in {ipcd,netpipe,np-server,np-client}/Dockerfile; do
	sed -e "s/{{USER}}/$USER/g" $f.in > $f
done

update "Generating IPCD container..."
docker build -q -t $USER/ipcd ipcd > /dev/null

update "Generating netpipe base container..."
docker build -q -t $USER/netpipe netpipe > /dev/null

update "Generating server container..."
docker build -q -t $USER/np-server np-server > /dev/null
update "Generating client container..."
docker build -q -t $USER/np-client np-client > /dev/null

update "Generating versions without any of our tech..."
DISABLE_CMD="rm /etc/ld.so.preload /bin/ipcd /lib/libipc.so"
DISABLE_CMD="$DISABLE_CMD && rm -rf /tmp/ipcd*"
for t in {server,client}; do
  rm -rf ./tmp
  mkdir -p tmp
  cp np-$t/${t}.sh tmp/
  echo "RUN $DISABLE_CMD" | cat np-$t/Dockerfile - > tmp/Dockerfile
  docker build -q -t $USER/np-$t-clean tmp
done
rm -rf ./tmp

status "Starting containers for IPCD experiment"

update "Starting IPCD container..."
docker run -d --name ipcd --volume /tmp $USER/ipcd > /dev/null
sleep 1

update "Starting server with ipcd socket mounted into it..."
docker run -d --name np-server --volumes-from ipcd $USER/np-server > /dev/null
sleep 1
update "Starting client container linked to server, also with ipcd socket mounted..."
docker run -d --name np-client --volumes-from ipcd --link np-server:server $USER/np-client > /dev/null

status "Monitor experiment (ipcd enabled)"
docker attach np-client
update "Transfer complete."

status "IPCD experiment cleanup, please be patient..."
update "Waiting for cient to ensure it's done:"
docker wait np-client >/dev/null
update "Stopping server and client containers..."
docker stop -t=1 np-server np-client > /dev/null
update "And removing their disk images..."
docker rm np-server np-client > /dev/null
update "Stopping ipcd container..."
docker stop -t=1 ipcd > /dev/null
update "And removing its disk image as well..."
docker rm ipcd > /dev/null

status "Starting containers for non-IPCD experiment:"
update "Starting server container..."
docker run -d --name np-server-clean $USER/np-server-clean > /dev/null
sleep 1
update "Starting client container linked to server..."
docker run -d --name np-client-clean --link np-server-clean:server $USER/np-client-clean > /dev/null

status "Monitor experiment (ipcd *NOT* enabled)"
docker attach np-client-clean
update "Transfer complete."

status "Cleanup! Please be patient..."
update "Waiting for cient to ensure it's done:"
docker wait np-client-clean >/dev/null

update "Stopping server and client containers..."
docker stop -t=1 np-server-clean np-client-clean > /dev/null
update "And removing their disk images..."
docker rm np-server-clean np-client-clean > /dev/null

echo
status "** Done! ** "
echo
