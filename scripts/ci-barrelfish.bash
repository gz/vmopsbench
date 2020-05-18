#!/bin/bash
set -ex

pip3 install -r scripts/requirements.txt

make clean
make

rm *.log *.csv *.png *.pdf /dev/shm/vmops_bench_* || true
sudo sysctl -w vm.max_map_count=50000000
#echo 192 | sudo tee  /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages


NR_2M_PAGES=16896
NR_1G_Pages=33

if [ -d /sys/kernel/mm/hugepages/hugepages-1048576kB ]; then
	echo "Setting $NR_1G_Pages 1GB Pages"
	echo 33 | sudo tee /sys/kernel/mm/hugepages/hugepages-1048576kB/nr_hugepages
	NRHUGEPAGES=$(cat /sys/kernel/mm/hugepages/hugepages-1048576kB/nr_hugepages)
	if [[ $NRHUGEPAGES == 0 ]]; then
		echo "Setting $NR_2M_PAGES 2MB Pages"
		echo 16896 | sudo tee /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages
	fi
elif [ -d /sys/kernel/mm/hugepages/hugepages-2048kB ]; then
	echo "Setting $NR_2M_PAGES 2MB Pages"
	echo $NR_2M_PAGES | sudo tee /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages
else
	echo "NO HUGEPAGES AVAILABLE"
fi

cat /sys/kernel/mm/hugepages/hugepages-1048576kB/nr_hugepages
cat /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages


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
if [ "$CI_MACHINE_TYPE" != "skylake4x" ]; then
	echo "Not running barrelfish on this machine..."
	exit 0
fi



if [[ "$1" = "throughput" ]]; then

	for benchmark in $benchmarks; do
		echo $benchmark

		CSVFILE_ALL=vmops_barrelfish_${benchmark}_threads_all_results.csv
		echo "thread_id,benchmark,core,ncores,memsize,numainterleave,mappings_size,page_size,memobj,isolation,duration,operations" | tee $CSVFILE_ALL

		LOGFILE=vmops_barrelfish_${benchmark}_threads_1_logfile.log
		CSVFILE=vmops_barrelfish_${benchmark}_threads_1_results.csv
		(python3 scripts/run_barrelfish.py      --benchmark $benchmark  --csvthpt "$CSVFILE" --csvlat "tmp.csv" --cores 1 --hake --time $BF_DURATION | tee $LOGFILE) || true
		tail -n +2 $CSVFILE >> $CSVFILE_ALL

		for corecount in 2 4 8 16; do
		    cores=`seq 0 1 $corecount`
			echo "$benchmark with $corecount cores"

			LOGFILE=vmops_barrelfish_${benchmark}_threads_${corecount}_logfile.log
			CSVFILE=vmops_barrelfish_${benchmark}_threads_${corecount}_results.csv

		    (python3 scripts/run_barrelfish.py --csvthpt "$CSVFILE" --csvlat "tmp.csv" --benchmark $benchmark --cores $corecount --time $BF_DURATION | tee $LOGFILE) || true

			if [ -f $CSVFILE ]; then
			    tail -n +2 $CSVFILE >> $CSVFILE_ALL
		    else
		    	echo "WARNING: $CSVFILE does not exists!!"
		    fi
		done
	done

elif [[ "$1" = "latency" ]]; then

	for benchmark in $benchmarks; do
		echo $benchmark


		THPT_CSVFILE_ALL=vmops_barrelfish_${benchmark}_threads_all_latency_results.csv
		LATENCY_CSVFILE_ALL=vmops_barrelfish_${benchmark}_threads_all_throughput_results.csv

		echo "thread_id,benchmark,core,ncores,memsize,numainterleave,mappings_size,page_size,memobj,isolation,duration,operations" | tee $THPT_CSVFILE_ALL
		echo "benchmark,core,ncores,memsize,numainterleave,mappings_size,page_size,memobj,isolation,threadid,elapsed,couter,latency" | tee $LATENCY_CSVFILE_ALL

		LOGFILE=vmops_barrelfish_${benchmark}_threads_1_latency_logfile.log
		THPT_CSVFILE=vmops_barrelfish_${benchmark}_threads_1_throughput_results.csv
		LATENCY_CSVFILE=vmops_barrelfish_${benchmark}_threads_1_latency_results.csv

		(python3 scripts/run_barrelfish.py --csvthpt "$THPT_CSVFILE" --csvlat "$LATENCY_CSVFILE" --nops $BF_SAMPLES --benchmark $benchmark --cores 1 --time $BF_DURATION --hake | tee $LOGFILE) || true

		tail -n +2 $THPT_CSVFILE >> $THPT_CSVFILE_ALL
		tail -n +2 $LATENCY_CSVFILE >> $LATENCY_CSVFILE_ALL

		for corecount in 2 4 8 16; do

			LOGFILE=vmops_barrelfish_${benchmark}_threads_${corecount}_latency_logfile.log
			THPT_CSVFILE=vmops_barrelfish_${benchmark}_threads_${corecount}_throughput_results.csv
			LATENCY_CSVFILE=vmops_barrelfish_${benchmark}_threads_${corecount}_latency_results.csv

	#        cores=`seq 0 1 $corecount`
		   	echo "$benchmark with $corecount cores"
		    (python3 scripts/run_barrelfish.py --csvthpt "$THPT_CSVFILE" --csvlat "$LATENCY_CSVFILE" --nops $BF_SAMPLES --benchmark $benchmark --cores $corecount --time $BF_DURATION | tee $LOGFILE) || true


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
	echo "ERROR: UNKNOWN ARGUMENT $1"
	exit 1
fi


rm -rf gh-pages
git clone -b gh-pages git@vmops-gh-pages:gz/vmops-bench.git gh-pages

export GIT_REV_CURRENT=`git rev-parse --short HEAD`
export CSV_LINE="`date +%Y-%m-%d`",${GIT_REV_CURRENT},"${CI_MACHINE_TYPE}/${GIT_REV_CURRENT}/index.html"
echo $CSV_LINE >> gh-pages/_data/$CI_MACHINE_TYPE.csv

DEPLOY_DIR="gh-pages/vmops/${CI_MACHINE_TYPE}/${GIT_REV_CURRENT}/"
mkdir -p ${DEPLOY_DIR}
cp gh-pages/vmops/index.markdown ${DEPLOY_DIR}


ls -lh

gzip *.csv
gzip *.log
mv *.log.gz ${DEPLOY_DIR}
mv *.csv.gz ${DEPLOY_DIR}
#mv *.pdf ${DEPLOY_DIR} 
#mv *.png ${DEPLOY_DIR}


cd gh-pages
git add .
git commit -a -m "Added benchmark results for $GIT_REV_CURRENT."
git push origin gh-pages
cd ..
rm -rf gh-pages
git clean -f


if [ -d /sys/kernel/mm/hugepages/hugepages-1048576kB ]; then
	echo 0 | sudo tee /sys/kernel/mm/hugepages/hugepages-1048576kB/nr_hugepages
elif [ -d /sys/kernel/mm/hugepages/hugepages-2048kB ]; then
	echo 0 | sudo tee /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages
fi
