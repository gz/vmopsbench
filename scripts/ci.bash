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

SAMPLES=100000
DURATION_MS=10000
MAX_CORES=`nproc`

#benchmarks="protect-isolated-shared mapunmap-shared-isolated maponly-isolated-shared elevate-isolated-shared"
benchmarks="elevate-isolated-shared maponly-isolated-shared"
numa=''
huge=''
memsize='4096'

BF_DURATION=3000
BF_SAMPLES=10000

# Run Barrelfish experiments
if [ "$CI_MACHINE_TYPE" = "skylake4x" ]; then

for benchmark in $benchmarks; do
	echo $benchmark 
	
	CSVFILE_ALL=vmops_barrelfish_${benchmark}_threads_all_results.csv
	echo "thread_id,benchmark,core,ncores,memsize,numainterleave,mappings_size,page_size,memobj,isolation,duration,operations" | tee $CSVFILE_ALL
	
	LOGFILE=vmops_barrelfish_${benchmark}_threads_1_logfile.log
	CSVFILE=vmops_barrelfish_${benchmark}_threads_1_results.csv
	python3 scripts/run_barrelfish.py --verbose --benchmark $benchmark  --csvthpt "$CSVFILE" --csvlat "tmp.csv" --cores 1 --verbose --hake --time $BF_DURATION || true
	tail -n +2 $CSVFILE >> $CSVFILE_ALL	

    for corecount in 2 4 8 16; do
        cores=`seq 0 1 $corecount`
		echo "$benchmark with $corecount cores"
       	
		LOGFILE=vmops_barrelfish_${benchmark}_threads_${corecount}_logfile.log
		CSVFILE=vmops_barrelfish_${benchmark}_threads_${corecount}_results.csv
       	
        python3 scripts/run_barrelfish.py --verbose  --csvthpt "$CSVFILE" --csvlat "tmp.csv" --benchmark $benchmark --cores $corecount --time $BF_DURATION || true
        
		if [ -f $CSVFILE ]; then
	        tail -n +2 $CSVFILE >> $CSVFILE_ALL
        else 
        	echo "WARNING: $CSVFILE does not exists!!"
        fi
    done
done

for benchmark in $benchmarks; do
	echo $benchmark
	
	
	THPT_CSVFILE_ALL=vmops_barrelfish_${benchmark}_threads_all_latency_results.csv
	LATENCY_CSVFILE_ALL=vmops_barrelfish_${benchmark}_threads_all_throughput_results.csv
	
    echo "thread_id,benchmark,core,ncores,memsize,numainterleave,mappings_size,page_size,memobj,isolation,duration,operations" | tee $THPT_CSVFILE_ALL
    echo "benchmark,core,ncores,memsize,numainterleave,mappings_size,page_size,memobj,isolation,threadid,elapsed,couter,latency" | tee $LATENCY_CSVFILE_ALL
	
	LOGFILE=vmops_barrelfish_${benchmark}_threads_1_latency_logfile.log
	THPT_CSVFILE=vmops_barrelfish_${benchmark}_threads_1_throughput_results.csv
	LATENCY_CSVFILE=vmops_barrelfish_${benchmark}_threads_1_latency_results.csv
	
    python3 scripts/run_barrelfish.py --verbose --csvthpt "$THPT_CSVFILE" --csvlat "$LATENCY_CSVFILE" --nops $BF_SAMPLES --benchmark $benchmark --cores 1 --time $BF_DURATION --verbose --hake || true

    tail -n +2 $THPT_CSVFILE >> $THPT_CSVFILE_ALL
    tail -n +2 $LATENCY_CSVFILE >> $LATENCY_CSVFILE_ALL
        
    for corecount in 2 4 8 16; do

		LOGFILE=vmops_barrelfish_${benchmark}_threads_${corecount}_latency_logfile.log
		THPT_CSVFILE=vmops_barrelfish_${benchmark}_threads_${corecount}_throughput_results.csv
		LATENCY_CSVFILE=vmops_barrelfish_${benchmark}_threads_${corecount}_latency_results.csv
	    
#        cores=`seq 0 1 $corecount`
       	echo "$benchmark with $corecount cores"
        python3 scripts/run_barrelfish.py --verbose --csvthpt "$THPT_CSVFILE" --csvlat "$LATENCY_CSVFILE" --nops $BF_SAMPLES --benchmark $benchmark --cores $corecount --time $BF_DURATION || true
        
        
        if [ -f $THPT_CSVFILE ]; then
	        tail -n +2 $THPT_CSVFILE >> $THPT_CSVFILE_ALL
        else 
        	echo "WARNING: $THPT_CSVFILE does not exists!!"        
        fi

        if [ -f $LATENCY_CSVFILE ]; then
	        tail -n +2 $LATENCY_CSVFILE >> $LATENCY_CSVFILE_ALL
        else 
        	echo "WARNING: $LATENCY_CSVFILE does not exists!!"
        fi

        
    done
    
    rm -rf $LATENCY_CSVFILE_ALL
done


else
    echo "skip bf on skylake2x for now"
## Ideally we build this stuff in docker (since we require ubuntu 19.10 but skylake2x are on 18.04):
## BF_SOURCE=$(readlink -f `pwd`)
## BF_BUILD=$BF_SOURCE/build
## BF_DOCKER=achreto/barrelfish-ci:20.04-lts
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



for benchmark in $benchmarks; do
    echo $benchmark
    
    if [[ "$benchmark" = "elevate-isolated-shared" ]]; then
        memsz='40960000'
    else
        memsz='4096'
    fi
    
    CSVFILE_ALL=vmops_linux_${benchmark}_threads_all_results.csv
    echo "thread_id,benchmark,core,ncores,memsize,numainterleave,mappings_size,page_size,memobj,isolation,duration,operations" | tee $CSVFILE_ALL
    for cores in `seq 0 8 $MAX_CORES`; do
    
   	    LOGFILE=vmops_linux_${benchmark}_threads_${cores}_logfile.log
	    CSVFILE=vmops_linux_${benchmark}_threads_${cores}_results.csv
    
        cat /proc/interrupts | grep TLB | tee -a $LOGFILE;
        (./bin/vmops -p $cores -t $DURATION_MS -m $memsz -b ${benchmark} ${numa} ${huge} | tee $CSVFILE) 3>&1 1>&2 2>&3 | tee -a $LOGFILE
        
        tail -n +2 $CSVFILE >> $CSVFILE_ALL
    done
    python3 scripts/plot.py $CSVFILE_ALL

done


for benchmark in $benchmarks; do
    echo $benchmark

    if [[ "$benchmark" = "elevate-isolated-shared" ]]; then
        memsz='40960000'
    else
        memsz='4096'
    fi    
	
	LATENCY_CSVFILE_ALL=vmops_linux_${benchmark}_threads_all_latency_results.csv
	THPT_CSVFILE_ALL=vmops_linux_${benchmark}_threads_all_throughput_results.csv
	
    echo "thread_id,benchmark,core,ncores,memsize,numainterleave,mappings_size,page_size,memobj,isolation,duration,operations" | tee $THPT_CSVFILE_ALL
    echo "benchmark,core,ncores,memsize,numainterleave,mappings_size,page_size,memobj,isolation,threadid,elapsed,couter,latency" | tee $LATENCY_CSVFILE_ALL
    for cores in `seq 0 8 $MAX_CORES`; do
    
   	    LOGFILE=vmops_linux_${benchmark}_threads_${cores}_latency_logfile.log
	    THPT_CSVFILE=vmops_linux_${benchmark}_threads_${cores}_throughput_results.csv
	    LATENCY_CSVFILE=vmops_linux_${benchmark}_threads_${cores}_latency_results.csv
	    
        cat /proc/interrupts | grep TLB | tee -a $LOGFILE;
        (./bin/vmops -z $LATENCY_CSVFILE -p $cores -n $SAMPLES -s $SAMPLES -r 0 -m $memsz -b ${benchmark} ${numa} ${huge} | tee $THPT_CSVFILE) 3>&1 1>&2 2>&3 | tee -a $LOGFILE
        
        tail -n +2 $THPT_CSVFILE >> $THPT_CSVFILE_ALL
        tail -n +2 $LATENCY_CSVFILE >> $LATENCY_CSVFILE_ALL
    done
    
    python3 scripts/plot.py $THPT_CSVFILE_ALL
    python3 scripts/plot.py $LATENCY_CSVFILE_ALL
    
    rm -rf $LATENCY_CSVFILE_ALL
done


rm -rf gh-pages
git clone -b gh-pages git@vmops-gh-pages:gz/vmops-bench.git gh-pages

export GIT_REV_CURRENT=`git rev-parse --short HEAD`
export CSV_LINE="`date +%Y-%m-%d`",${GIT_REV_CURRENT},"${CI_MACHINE_TYPE}/${GIT_REV_CURRENT}/index.html"
echo $CSV_LINE >> gh-pages/_data/$CI_MACHINE_TYPE.csv

DEPLOY_DIR="gh-pages/vmops/${CI_MACHINE_TYPE}/${GIT_REV_CURRENT}/"
mkdir -p ${DEPLOY_DIR}
cp gh-pages/vmops/index.markdown ${DEPLOY_DIR}

gzip *.csv
gzip *.log
mv *.log.gz ${DEPLOY_DIR}
mv *.csv.gz ${DEPLOY_DIR}
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
