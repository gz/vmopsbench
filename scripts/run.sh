#!/bin/bash
set -ex
make -j 12

HOSTNAME=`hostname`
DURATION_MS=10000
MAX_CORES=`nproc`

declare -a bench=("mapunmap" "maponly" "protect")
declare -a isolation=("" "-isolated")
declare -a sharing=("independent" "shared")
declare -a mappings=("" "-4k")

declare -a memsizes=("4096" "2097152")
declare -a numa=("" "-i")
declare -a huge=("" "-l")

#echo never > /sys/kernel/mm/transparent_hugepage/enabled
rm *.log *.csv *.png *.pdf /dev/shm/vmops_bench_* || true
sudo sysctl -w vm.max_map_count=50000000
echo 100000 | sudo tee  /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages


for b in "${bench[@]}"; do
    for i in "${isolation[@]}"; do
        for s in "${sharing[@]}"; do
            for m in "${mappings[@]}"; do
                benchmark="$b$i-$s$m"
                for numa in "${numa[@]}"; do
                    for h in "${huge[@]}"; do
                        LOGFILE=${HOSTNAME}_results_${bench}.log
                        CSVFILE=${HOSTNAME}_results_${bench}.csv

                        for memsize in "${memsizes[@]}"; do
                            if [ ! -f "$CSVFILE" ]; then
                                echo "thread_id,benchmark,core,ncores,memsize,numainterleave,mappings_size,page_size,memobj,isolation,duration,operations" | tee $CSVFILE
                            fi
                            for cores in `seq 0 8 $MAX_CORES`; do
                                cat /proc/interrupts | grep TLB | tee -a $LOGFILE;
                                (./bin/vmops -p $cores -t $DURATION_MS -m $memsize -b ${benchmark} ${numa} ${h} | tee -a $CSVFILE) 3>&1 1>&2 2>&3 | tee -a $LOGFILE
                            done
                        done

                        #python3 ./scripts/plot.py ${CSVFILE}
                    done
                done
            done
        done
    done
done
