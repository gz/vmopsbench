#!/bin/bash
#
# Usage:
# - CI_MACHINE_TYPE=skylake2x bash scripts/ci-linux.bash latency
# - CI_MACHINE_TYPE=skylake2x bash scripts/ci-linux.bash throughput
#

set -ex

BASE="$(dirname "$0")"


pip3 install -r scripts/requirements.txt

make clean
make

SAMPLES=100000
DURATION_MS=10000
MAX_CORES=`nproc`

#benchmarks="protect-isolated-shared mapunmap-shared-isolated maponly-isolated-shared elevate-isolated-shared"
benchmarks="maponly-isolated-shared"
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

		CSVFILE_ALL=vmops_linux_${benchmark}_threads_all_throughput_results.csv
		echo "thread_id,benchmark,core,ncores,memsize,numainterleave,mappings_size,page_size,memobj,isolation,duration,operations" | tee $CSVFILE_ALL
		for cores in 1 `seq $increment $increment $MAX_CORES`; do

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

		THPT_CSVFILE_ALL=vmops_linux_${benchmark}_threads_all_latency_results.csv

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
