#!/bin/bash
set -ex
make -j 12

HOSTNAME=`hostname`
DURATION_MS=20000
MAX_CORES=`nproc`

declare -a benchmarks=("mapunmap-isolated" "mapunmap-independent" "mapunmap-shared-isolated" "mapunmap-shared-independent" "mapunmap-4k-isolated" "mapunmap-4k-independent" "protect-shared" "protect-independent" "protect-4k-independent")
declare -a memsizes=("4096" "2097152")

echo never > /sys/kernel/mm/transparent_hugepage/enabled
rm *.log *.csv /dev/shm/vmops_bench_* || true

for benchmark in "${benchmarks[@]}"; do
    LOGFILE=${HOSTNAME}_results_${benchmark}.log
    CSVFILE=${HOSTNAME}_results_${benchmark}.csv
    for memsize in "${memsizes[@]}"; do
        if [ ! -f "$CSVFILE" ]; then
            echo "thread_id,benchmark,core,ncores,memsize,duration,operations" | tee $CSVFILE
        fi

        for cores in `seq 0 4 $MAX_CORES`; do
            cat /proc/interrupts | grep TLB | tee -a $LOGFILE;
            (./bin/vmops -p $cores -t $DURATION_MS -m $memsize -b ${benchmark} | tee -a $CSVFILE) 3>&1 1>&2 2>&3 | tee -a $LOGFILE
        done
    done
    python3 ./scripts/plot.py ${CSVFILE}
done