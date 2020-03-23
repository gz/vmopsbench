#!/bin/bash
set -ex
make -j 12

HOSTNAME=`hostname`
DURATION_MS=20000
MAX_CORES=`nproc`

declare -a benchmarks=("mapunmap-isolated" "mapunmap-independent" "mapunmap-shared-isolated" "mapunmap-shared-independent" "mapunmap-4k-isolated" "mapunmap-4k-independent" "protect-shared" "protect-independent" "protect-4k-independent")

for benchmark in "${benchmarks[@]}"; do
    LOGFILE=${HOSTNAME}_results_${benchmark}.log
    CSVFILE=${HOSTNAME}_results_${benchmark}.csv
    echo "thread_id,benchmark,core,ncores,memsize,duration,operations" | tee -a $CSVFILE
    if test -f "$LOGFILE"; then
        echo "$LOGFILE with previous results already exists."
        exit 1
    fi

    for cores in `seq 0 4 $MAX_CORES`; do
        cat /proc/interrupts | grep TLB | tee -a $LOGFILE;
        (./bin/vmops -p $cores -t $DURATION_MS -b ${benchmark} | tee -a $CSVFILE) 3>&1 1>&2 2>&3 | tee -a $LOGFILE
    done
done
