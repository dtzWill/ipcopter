#!/bin/bash

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

status "Generating containers"

update "Configuring local Dockerfiles..."
for f in {ipcd,server,client}/Dockerfile; do
	sed -e "s/{{USER}}/$USER/g" $f.in > $f
done

update "Generating IPCD container..."
docker build -q -t $USER/ipcd ipcd > build.ipcd.log

update "Generating server container..."
docker build -q -t $USER/mem-server server > build.server.log
update "Generating client container..."
docker build -q -t $USER/mem-client client > build.client.log

update "Generating versions without any of our tech..."
DISABLE_CMD="rm /etc/ld.so.preload /bin/ipcd /lib/libipc.so"
DISABLE_CMD="$DISABLE_CMD && rm -rf /tmp/ipcd*"
for t in {server,client}; do
  mkdir -p tmp
  echo "RUN $DISABLE_CMD" | cat $t/Dockerfile - > tmp/Dockerfile
  docker build -q -t $USER/mem-$t-clean tmp
done
rm -rf ./tmp

status "Starting containers for IPCD experiment"

update "Starting IPCD container..."
docker run -d --name ipcd --volume /tmp $USER/ipcd > /dev/null
sleep 1

update "Starting server with ipcd socket mounted into it..."
docker run -d --name mem-server --volumes-from ipcd $USER/mem-server > /dev/null
sleep 1
update "Starting client container linked to server, also with ipcd socket mounted..."
docker run -d --name mem-client --volumes-from ipcd --link mem-server:server $USER/mem-client > /dev/null

status "Monitor experiment (ipcd enabled)"
docker attach mem-client
update "Transfer complete."

status "IPCD experiment cleanup, please be patient..."
update "Waiting for cient to ensure it's done:"
docker wait mem-client >/dev/null
update "Stopping server and client containers..."
docker stop -t=1 mem-server mem-client > /dev/null
update "And removing their disk images..."
docker rm mem-server mem-client > /dev/null
update "Stopping ipcd container..."
docker stop -t=1 ipcd > /dev/null
update "And removing its disk image as well..."
docker rm ipcd > /dev/null

status "Starting containers for non-IPCD experiment:"
update "Starting server container..."
docker run -d --name mem-server-clean $USER/mem-server-clean > /dev/null
sleep 1
update "Starting client container linked to server..."
docker run -d --name mem-client-clean --link mem-server-clean:server $USER/mem-client-clean > /dev/null

status "Monitor experiment (ipcd *NOT* enabled)"
docker attach mem-client-clean
update "Transfer complete."

status "Cleanup! Please be patient..."
update "Waiting for cient to ensure it's done:"
docker wait mem-client-clean >/dev/null

update "Stopping server and client containers..."
docker stop -t=1 mem-server-clean mem-client-clean > /dev/null
update "And removing their disk images..."
docker rm mem-server-clean mem-client-clean > /dev/null

echo
status "** Done! ** "
echo
