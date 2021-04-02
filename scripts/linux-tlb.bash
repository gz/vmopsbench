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
benchmarks="tlbshoot"
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

if [[ "$1" = "latency" ]]; then

	for benchmark in $benchmarks; do
		echo $benchmark

		if [[ "$benchmark" = "elevate-isolated-shared" ]]; then
		    memsz='40960000'
		else
		    memsz='4096'
		fi

		THPT_CSVFILE_ALL=tlb_linux_${benchmark}_threads_all_throughput_results.csv

		echo "thread_id,benchmark,core,ncores,memsize,numainterleave,mappings_size,page_size,memobj,isolation,duration,operations" | tee $THPT_CSVFILE_ALL
		for cores in 1 `seq $increment $increment $MAX_CORES`; do

	   	    LOGFILE=tlb_linux_${benchmark}_threads_${cores}_latency_logfile.log
			THPT_CSVFILE=tlb_linux_${benchmark}_threads_${cores}_throughput_results.csv
			LATENCY_CSVFILE=tlb_linux_${benchmark}_threads_${cores}_latency_results.csv

			NUM_SAMPES=$SAMPLES
			if [[ $cores > 100 ]]; then
				NUM_SAMPES=2000
			elif [[ $cores > 50 ]]; then
			   	NUM_SAMPES=5000
			else
				NUM_SAMPES=10000
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


