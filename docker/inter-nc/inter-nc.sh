#!/bin/sh

# XXX: Assumes will/ipc-hook already exists

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



status "Generating containers"
update "Generating IPCD container..."
docker build -q -t $USER/ipcd ipcd > /dev/null

update "Generating server container..."
docker build -q -t $USER/nc-server nc-server > /dev/null
update "Generating client container..."
docker build -q -t $USER/nc-client nc-client > /dev/null

status "Starting containers for IPCD experiment"

update "Starting IPCD container..."
docker run -d --name ipcd --volume /tmp $USER/ipcd > /dev/null
sleep 1

update "Starting server with ipcd socket mounted into it..."
docker run -d --name nc-server --volumes-from ipcd $USER/nc-server > /dev/null
sleep 1
update "Starting client container linked to server, also with ipcd socket mounted..."
docker run -d --name nc-client --volumes-from ipcd --link nc-server:server $USER/nc-client > /dev/null

status "Monitor experiment (ipcd enabled)"
docker attach nc-server
update "Transfer complete."

status "IPCD experiment cleanup, please be patient..."
update "Waiting for cient to ensure it's done:"
docker wait nc-client >/dev/null
update "Stopping server and client containers..."
docker stop -t=1 nc-server nc-client > /dev/null
update "And removing their disk images..."
docker rm nc-server nc-client > /dev/null
update "Stopping ipcd container..."
docker stop -t=1 ipcd > /dev/null
update "And removing its disk image as well..."
docker rm ipcd > /dev/null

status "Starting containers for non-IPCD experiment:"
update "Starting server container..."
docker run -d --name nc-server $USER/nc-server > /dev/null
sleep 1
update "Starting client container linked to server..."
docker run -d --name nc-client --link nc-server:server $USER/nc-client > /dev/null

status "Monitor experiment (ipcd *NOT* enabled)"
docker attach nc-server
update "Transfer complete."

status "Cleanup! Please be patient..."
update "Waiting for cient to ensure it's done:"
docker wait nc-client >/dev/null

update "Stopping server and client containers..."
docker stop -t=1 nc-server nc-client > /dev/null
update "And removing their disk images..."
docker rm nc-server nc-client > /dev/null

echo
status "** Done! ** "
echo
