#!/bin/bash
set -ex

pip3 install -r scripts/requirements.txt

make clean
make

rm *.log *.csv *.png *.pdf /dev/shm/vmops_bench_* || true
sudo sysctl -w vm.max_map_count=50000000
#echo 192 | sudo tee  /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages


NR_2M_PAGES=25088
NR_1G_Pages=49

# if [ -d /sys/kernel/mm/hugepages/hugepages-1048576kB ]; then
# 	echo "Setting $NR_1G_Pages 1GB Pages"
# 	echo $NR_1G_Pages | sudo tee /sys/kernel/mm/hugepages/hugepages-1048576kB/nr_hugepages
# 	NRHUGEPAGES=$(cat /sys/kernel/mm/hugepages/hugepages-1048576kB/nr_hugepages)
# 	if [[ $NRHUGEPAGES == 0 ]]; then
# 		echo "Setting $NR_2M_PAGES 2MB Pages"
# 		echo $NR_2M_PAGES | sudo tee /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages
# 	fi
# elif [ -d /sys/kernel/mm/hugepages/hugepages-2048kB ]; then
# 	echo "Setting $NR_2M_PAGES 2MB Pages"
# 	echo $NR_2M_PAGES | sudo tee /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages
# else
# 	echo "NO HUGEPAGES AVAILABLE"
# fi

# cat /sys/kernel/mm/hugepages/hugepages-1048576kB/nr_hugepages
# cat /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages


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

if [[ $MAX_CORE -gt 90 ]]; then
	increment=4
elif [[ $MAX_CORE -gt 24 ]]; then
	increment=4
else
	increment=2
fi

if [[ "$1" = "throughput" ]]; then

	for benchmark in $benchmarks; do

		CSVFILE_ALL=vmops_barrelfish_${benchmark}_threads_all_results.csv
		echo "thread_id,benchmark,core,ncores,memsize,numainterleave,mappings_size,page_size,memobj,isolation,duration,operations" | tee $CSVFILE_ALL

		LOGFILE=vmops_barrelfish_${benchmark}_threads_1_logfile.log
		CSVFILE=vmops_barrelfish_${benchmark}_threads_1_results.csv
		(python3 scripts/run_barrelfish.py --verbose     --benchmark $benchmark  --csvthpt "$CSVFILE" --csvlat "tmp.csv" --cores 1 --hake --time $BF_DURATION || true) | tee $LOGFILE
		if [ -f $CSVFILE ]; then
			tail -n +2 $CSVFILE >> $CSVFILE_ALL
		fi;
		for corecount in `seq $increment $increment $MAX_CORES`; do
			echo "$benchmark with $corecount cores"

			LOGFILE=vmops_barrelfish_${benchmark}_threads_${corecount}_logfile.log
			CSVFILE=vmops_barrelfish_${benchmark}_threads_${corecount}_results.csv

			(python3 scripts/run_barrelfish.py --verbose  --csvthpt "$CSVFILE" --csvlat "tmp.csv" --benchmark $benchmark --cores $corecount --time $BF_DURATION || true) | tee $LOGFILE

			if [ -f $CSVFILE ]; then
			    tail -n +2 $CSVFILE >> $CSVFILE_ALL
		    else
		    	echo "WARNING: $CSVFILE does not exists!!"
		    fi
		done
	done

elif [[ "$1" = "latency" ]]; then

	for benchmark in $benchmarks; do

		THPT_CSVFILE_ALL=vmops_barrelfish_${benchmark}_threads_all_latency_results.csv
		echo "thread_id,benchmark,core,ncores,memsize,numainterleave,mappings_size,page_size,memobj,isolation,duration,operations" | tee $THPT_CSVFILE_ALL

		LOGFILE=vmops_barrelfish_${benchmark}_threads_1_latency_logfile.log
		THPT_CSVFILE=vmops_barrelfish_${benchmark}_threads_1_throughput_results.csv
		LATENCY_CSVFILE=vmops_barrelfish_${benchmark}_threads_1_latency_results.csv

		(python3 scripts/run_barrelfish.py --verbose --csvthpt "$THPT_CSVFILE" --csvlat "$LATENCY_CSVFILE" --nops $BF_SAMPLES --benchmark $benchmark --cores 1 --time $BF_DURATION --hake || true) | tee $LOGFILE
		if [ -f $THPT_CSVFILE ]; then
			tail -n +2 $THPT_CSVFILE >> $THPT_CSVFILE_ALL
		fi

		for corecount in 1 `seq 8 8 $MAX_CORES`; do

			LOGFILE=vmops_barrelfish_${benchmark}_threads_${corecount}_latency_logfile.log
			THPT_CSVFILE=vmops_barrelfish_${benchmark}_threads_${corecount}_throughput_results.csv
			LATENCY_CSVFILE=vmops_barrelfish_${benchmark}_threads_${corecount}_latency_results.csv

	#        cores=`seq 0 1 $corecount`
		   	echo "$benchmark with $corecount cores"
			(python3 scripts/run_barrelfish.py --verbose --csvthpt "$THPT_CSVFILE" --csvlat "$LATENCY_CSVFILE" --nops $BF_SAMPLES --benchmark $benchmark --cores $corecount --time $BF_DURATION || true ) | tee $LOGFILE


		    if [ -f $THPT_CSVFILE ]; then
			    tail -n +2 $THPT_CSVFILE >> $THPT_CSVFILE_ALL
		    else
		    	echo "WARNING: $THPT_CSVFILE does not exists!!"
		    fi

		    if [ -f $LATENCY_CSVFILE ]; then
				python3 scripts/histogram.py $LATENCY_CSVFILE Barrelfish || true
		    else
		    	echo "WARNING: $LATENCY_CSVFILE does not exists!!"
		    fi
		done
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

gzip *.csv || true
gzip *.log || true
mv *.log.gz ${DEPLOY_DIR} || true
mv *.csv.gz ${DEPLOY_DIR} || true
mv *.pdf ${DEPLOY_DIR} || true
mv *.png ${DEPLOY_DIR} || true

cd gh-pages
git add .
git commit -a -m "Added benchmark results for $GIT_REV_CURRENT."

#doing a git fetch/rebase here in case there has been a push in mean time
git fetch
git rebase
git push origin gh-pages
cd ..
rm -rf gh-pages
git clean -f


# if [ -d /sys/kernel/mm/hugepages/hugepages-1048576kB ]; then
# 	echo 0 | sudo tee /sys/kernel/mm/hugepages/hugepages-1048576kB/nr_hugepages
# elif [ -d /sys/kernel/mm/hugepages/hugepages-2048kB ]; then
# 	echo 0 | sudo tee /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages
# fi
