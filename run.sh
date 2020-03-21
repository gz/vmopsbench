#!/bin/bash
set -ex
make -j 12

HOSTNAME=`hostname`
DURATION_MS=5000
MAX_CORES=`nproc`
declare -a benchmarks=("independent" "shared-independent" "shared-isolated" "concurrent-protect")

for benchmark in "${benchmarks[@]}"; do
    LOGFILE=${HOSTNAME}_results_${benchmark}.log
    if test -f "$LOGFILE"; then
        echo "$LOGFILE with previous results already exists."
        exit 1
    fi

    for cores in `seq 0 4 $MAX_CORES`; do
        cat /proc/interrupts | grep TLB | tee -a $LOGFILE;
        ./bin/vmops -p $cores -t $DURATION_MS -b ${benchmark} | tee -a $LOGFILE;
    done
done
