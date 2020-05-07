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

DURATION_MS=10000
MAX_CORES=`nproc`

benchmark='maponly-isolated-independent-4k'
numa=''
huge=''
memsize='4096'

LOGFILE=results_${benchmark}.log
CSVFILE=results_${benchmark}.csv

# Run benchmarks here
python3 scripts/plot.py $CSVFILE


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
