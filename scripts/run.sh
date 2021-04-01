#!/bin/bash
set -ex 

rm *.log *.csv *.png *.pdf /dev/shm/vmops_bench_* || true
sudo umount -f /mnt || true

export GIT_HASH=`git rev-parse --short HEAD`
benchmarks='mixX0 mixX10 mixX60 mixX100'
CSVFILE=fsops_benchmark.csv

for benchmark in $benchmarks; do
    # Mount tmpfs
    sudo mount tmpfs /mnt -t tmpfs

    RUST_TEST_THREADS=1 cargo bench --bench fxmark -- --duration 10 --type $benchmark

    # Unmount tmpfs
    sudo umount -f /mnt
done

