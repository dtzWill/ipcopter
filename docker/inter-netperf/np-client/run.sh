#!/bin/bash

set -e

CONFID="95,5"

SIZES="$(python -c "print ' '.join(map(lambda x:str(2**x),range(21)))")"

case $WORKLOAD in
test)
  SIZES="4"
  ITERS="2,2"
  TIME=5
  ;;
basic)
  SIZES="$(python -c "print ' '.join(map(lambda x:str(8**x),range(8)))")"
  ITERS="3,7"
  TIME=10
  ;;
full)
  SIZES="$(python -c "print ' '.join(map(lambda x:str(2**x),range(21)))")"
  ITERS="3,30"
  TIME=10
  ;;
*)
  echo "Unrecognized WORKLOAD '$WORKLOAD'"
  exit 1
esac

RESULTS=$PWD/results
rm -rf $RESULTS || /bin/true
mkdir -p $RESULTS

for size in $SIZES; do
  LOG=$RESULTS/${size}.log
  stdbuf -oL -eL netperf -t TCP_STREAM -l $TIME -fM -I $CONFID -i $ITERS -H server -p 3000 -- -m $size |& tee $LOG
done

RESULT_CSV=$RESULTS/results.csv

echo '"Recv Socket Size","Send Socket Size","Send Message Size", "Elapsed Time", "Throughput MB/s"' > $RESULT_CSV

for size in $SIZES; do
  LOG=$RESULTS/${size}.log
  tail -n1 $LOG | awk '{$1=$1}1' OFS="," >> $RESULT_CSV
done
