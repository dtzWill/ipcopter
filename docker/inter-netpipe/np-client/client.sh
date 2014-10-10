#!/bin/bash

set -e

ROOT=$(readlink -f $(dirname $0))

case $WORKLOAD in
test)
  MAX_CHUNK_SIZE=4
  ITERS=1
  ;;
full | basic)
  #SIZES= (default)
  MAX_CHUNK_SIZE=16777216
  ITERS=5
  ;;
*)
  echo "Unrecognized WORKLOAD '$WORKLOAD'"
  exit 1
esac

RESULTS=/results
rm -rf $RESULTS || true
mkdir -p $RESULTS

echo '"Iteration", "Perturbation", "Bytes","Throughput (Mbps)","Time (s)"' > $RESULTS/results.csv

for i in $(seq $ITERS); do
# Cross fingers we don't need to specify port...
  let 'PORT=3000+i'
  OUT=$RESULTS/np.${i}.out

  # Server
  # ./NPtcp -l 1 -u $MAX_CHUNK_SIZE -P$PORT -p3 &

  # Client
  sleep 1
  ./NPtcp -h server -o $OUT -l 1 -u $MAX_CHUNK_SIZE -P$PORT -p3

  awk '{$1=$1}1' OFS=',' < $OUT | sed "s/^/$i,$i,/" >> $RESULTS/results.csv
done
