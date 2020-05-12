#!/bin/bash
set -ex

pip3 install -r scripts/requirements.txt

make clean
make

make profile-maponly-default
make profile-maponly-isolated
make profile-maponly-default-4
make profile-maponly-isolated-4

rm *.log *.csv *.png *.pdf /dev/shm/vmops_bench_* || true
sudo sysctl -w vm.max_map_count=50000000
echo 192 | sudo tee  /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages

SAMPLES=1000
DURATION_MS=10000
MAX_CORES=`nproc`

benchmarks="protect-isolated-shared elevate-isolated-shared mapunmap-shared-isolated maponly-isolated-shared"
numa=''
huge=''
memsize='4096'

for benchmark in $benchmarks; do
    echo $benchmark

    LOGFILE=results_${benchmark}.log
    CSVFILE=results_${benchmark}.csv

    echo "thread_id,benchmark,core,ncores,memsize,numainterleave,mappings_size,page_size,memobj,isolation,duration,operations" | tee $CSVFILE
    for cores in `seq 0 8 $MAX_CORES`; do
        cat /proc/interrupts | grep TLB | tee -a $LOGFILE;
        (./bin/vmops -p $cores -t $DURATION_MS -m $memsize -b ${benchmark} ${numa} ${huge} | tee -a $CSVFILE) 3>&1 1>&2 2>&3 | tee -a $LOGFILE
    done
    python3 scripts/plot.py $CSVFILE

done


for benchmark in $benchmarks; do
    echo $benchmark

    LOGFILE=results_${benchmark}_latency.log
    THPT_CSVFILE=results_${benchmark}_throughput.csv
    LATENCY_CSVFILE=results_${benchmark}_latency.csv

    echo "thread_id,benchmark,core,ncores,memsize,numainterleave,mappings_size,page_size,memobj,isolation,duration,operations" | tee $THPT_CSVFILE
    echo "benchmark,core,ncores,memsize,numainterleave,mappings_size,page_size,memobj,isolation,threadid,elapsed,couter,latency" | tee $LATENCY_CSVFILE
    for cores in `seq 0 8 $MAX_CORES`; do
        cat /proc/interrupts | grep TLB | tee -a $LOGFILE;
        (./bin/vmops -z $LATENCY_CSVFILE -p $cores -n $SAMPLES -s $SAMPLES -r 0 -m $memsize -b ${benchmark} ${numa} ${huge} | tee -a $THPT_CSVFILE) 3>&1 1>&2 2>&3 | tee -a $LOGFILE
    done
    python3 scripts/plot.py $THPT_CSVFILE
    python3 scripts/plot.py $LATENCY_CSVFILE
done


# Run Barrelfish experiments
if [ "$CI_MACHINE_TYPE" = "skylake4x" ]; then

for benchmark in $benchmarks; do
    python3 scripts/run_barrelfish.py --benchmark $benchmark --cores 1 --verbose --hake
    for corecount in 2 4 8 16 24 32; do
    	cores=`seq 0 1 $corecount`
		python3 scripts/run_barrelfish.py --benchmark $benchmark --cores $cores
    done
done

for benchmark in $benchmarks; do
    python3 scripts/run_barrelfish.py --benchmark $benchmark --cores 1 --verbose --hake
    for corecount in 2 4 8 16 24 32; do
    	cores=`seq 0 1 $corecount`
		python3 scripts/run_barrelfish.py --nops $SAMPLES --benchmark $benchmark --cores $cores
    done
done

else
    echo "skip bf on skylake2x for now"
## Ideally we build this stuff in docker (since we require ubuntu 19.10 but skylake2x are on 18.04):
## BF_SOURCE=$(readlink -f `pwd`)
## BF_BUILD=$BF_SOURCE/build
## BF_DOCKER=achreto/barrelfish-ci
## echo "bfdocker: $BF_DOCKER"
## echo "bfsrc: $BF_SOURCE  build: $BF_BUILD"
## # pull the docker image
## docker pull $BF_DOCKER
## # create the build directory
## mkdir -p $BF_BUILD
## # run the command in the docker image
## docker run -u $(id -u) -i -t \
##    --mount type=bind,source=$BF_SOURCE,target=/source \
##    --mount type=bind,source=$BF_BUILD,target=/source/build \
##    $BF_DOCKER
fi

rm -rf gh-pages
git clone -b gh-pages git@vmops-gh-pages:gz/vmops-bench.git gh-pages

export GIT_REV_CURRENT=`git rev-parse --short HEAD`
export CSV_LINE="`date +%Y-%m-%d`",${GIT_REV_CURRENT},"${CI_MACHINE_TYPE}/${GIT_REV_CURRENT}/index.html"
echo $CSV_LINE >> gh-pages/_data/$CI_MACHINE_TYPE.csv

DEPLOY_DIR="gh-pages/vmops/${CI_MACHINE_TYPE}/${GIT_REV_CURRENT}/"
mkdir -p ${DEPLOY_DIR}
cp gh-pages/vmops/index.markdown ${DEPLOY_DIR}

mv *.log ${DEPLOY_DIR}
mv *.csv ${DEPLOY_DIR}
mv *.pdf ${DEPLOY_DIR}
mv *.png ${DEPLOY_DIR}
cp perfdata/*.svg ${DEPLOY_DIR}

cd gh-pages
git add .
git commit -a -m "Added benchmark results for $GIT_REV_CURRENT."
git push origin gh-pages
cd ..
rm -rf gh-pages
git clean -f
