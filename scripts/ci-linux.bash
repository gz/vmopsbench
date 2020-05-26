#!/bin/bash
#
# Usage: 
# - CI_MACHINE_TYPE=skylake2x bash scripts/ci-linux.bash latency
# - CI_MACHINE_TYPE=skylake2x bash scripts/ci-linux.bash throughput
#

set -ex

pip3 install -r scripts/requirements.txt

make clean
make

SAMPLES=100000
DURATION_MS=10000
MAX_CORES=`nproc`

#benchmarks="protect-isolated-shared mapunmap-shared-isolated maponly-isolated-shared elevate-isolated-shared"
benchmarks="elevate-isolated-shared maponly-isolated-shared"
numa=''
huge=''
memsize='4096'

if [[ $MAX_CORE -gt 90 ]]; then
	 increment=8
elif [[ $MAX_CORE -gt 24 ]]; then
	increment=4
else
	increment=2
fi

if [[ "$1" = "throughput" ]]; then

	for benchmark in $benchmarks; do
		echo $benchmark

		if [[ "$benchmark" = "elevate-isolated-shared" ]]; then
		    memsz='40960000'
		else
		    memsz='4096'
		fi

		CSVFILE_ALL=vmops_linux_${benchmark}_threads_all_results.csv
		echo "thread_id,benchmark,core,ncores,memsize,numainterleave,mappings_size,page_size,memobj,isolation,duration,operations" | tee $CSVFILE_ALL
		for cores in 1 `seq 0 $increment $MAX_CORES`; do

	   	    LOGFILE=vmops_linux_${benchmark}_threads_${cores}_logfile.log
			CSVFILE=vmops_linux_${benchmark}_threads_${cores}_results.csv

		    cat /proc/interrupts | grep TLB | tee -a $LOGFILE;
		    (./bin/vmops -p $cores -t $DURATION_MS -m $memsz -b ${benchmark} ${numa} ${huge} | tee $CSVFILE) 3>&1 1>&2 2>&3 | tee -a $LOGFILE

		    tail -n +2 $CSVFILE >> $CSVFILE_ALL
		done
		python3 scripts/plot.py $CSVFILE_ALL

	done

elif [[ "$1" = "latency" ]]; then

	for benchmark in $benchmarks; do
		echo $benchmark

		if [[ "$benchmark" = "elevate-isolated-shared" ]]; then
		    memsz='40960000'
		else
		    memsz='4096'
		fi

		THPT_CSVFILE_ALL=vmops_linux_${benchmark}_threads_all_throughput_results.csv

		echo "thread_id,benchmark,core,ncores,memsize,numainterleave,mappings_size,page_size,memobj,isolation,duration,operations" | tee $THPT_CSVFILE_ALL
		for cores in 1 `seq 8 $increment $MAX_CORES`; do

	   	    LOGFILE=vmops_linux_${benchmark}_threads_${cores}_latency_logfile.log
			THPT_CSVFILE=vmops_linux_${benchmark}_threads_${cores}_throughput_results.csv
			LATENCY_CSVFILE=vmops_linux_${benchmark}_threads_${cores}_latency_results.csv

			NUM_SAMPES=$SAMPLES
			if [[ $cores > 100 ]]; then
				NUM_SAMPES=25000
			elif [[ $cores > 50 ]]; then
			   	NUM_SAMPES=50000
			else
				NUM_SAMPES=100000
			fi

		    cat /proc/interrupts | grep TLB | tee -a $LOGFILE;
		    (./bin/vmops -z $LATENCY_CSVFILE -p $cores -n $NUM_SAMPES -s $NUM_SAMPES -r 0 -m $memsz -b ${benchmark} ${numa} ${huge} | tee $THPT_CSVFILE) 3>&1 1>&2 2>&3 | tee -a $LOGFILE

			python3 scripts/histogram.py $LATENCY_CSVFILE Linux
			rm $LATENCY_CSVFILE
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

#make profile-maponly-default
#make profile-maponly-isolated
#make profile-maponly-default-4
#make profile-maponly-isolated-4
#cp perfdata/*.svg ${DEPLOY_DIR}

cd gh-pages
git add .
git commit -a -m "Added benchmark results for $GIT_REV_CURRENT."

#refetch in case there is an update
git fetch
git rebase
git push origin gh-pages
cd ..
rm -rf gh-pages
git clean -f

