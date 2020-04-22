#!/bin/bash
set -ex 

make clean
make

make profile-maponly-default
make profile-maponly-isolated
make profile-maponly-default-4
make profile-maponly-isolated-4

rm *.log *.csv *.png *.pdf /dev/shm/vmops_bench_* || true
sudo sysctl -w vm.max_map_count=50000000
echo 10 | sudo tee  /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages

HOSTNAME=`hostname`
DURATION_MS=10000
MAX_CORES=`nproc`

benchmark='maponly-isolated-independent-4k'
numa=''
huge=''
memsize='4096'

LOGFILE=${HOSTNAME}_results_${benchmark}.log
CSVFILE=${HOSTNAME}_results_${benchmark}.csv

echo "thread_id,benchmark,core,ncores,memsize,numainterleave,mappings_size,page_size,memobj,isolation,duration,operations" | tee $CSVFILE
for cores in `seq 0 8 $MAX_CORES`; do
    cat /proc/interrupts | grep TLB | tee -a $LOGFILE;
    (./bin/vmops -p $cores -t $DURATION_MS -m $memsize -b ${benchmark} ${numa} ${huge} | tee -a $CSVFILE) 3>&1 1>&2 2>&3 | tee -a $LOGFILE
done
python3 scripts/plot.py $CSVFILE

python3 scripts/run_barrelfish.py --cores 1 --verbose --hake
python3 scripts/run_barrelfish.py --cores 1,2
python3 scripts/run_barrelfish.py --cores 1,2,3
python3 scripts/run_barrelfish.py --cores 1,2,3,4

git clone -b gh-pages git@github.com:gz/vmops-bench.git gh-pages
