#!/bin/bash
set -ex 

pip3 install -r scripts/requirements.txt

rm *.log *.csv *.png *.pdf /dev/shm/vmops_bench_* || true
sudo umount -f /mnt || true
# Mount tmpfs
sudo mount tmpfs /mnt -t tmpfs

benchmark='drbl drbh dwol dwom mwrl mwrm dwal'
CSVFILE=fsops_benchmark.csv

RUST_TEST_THREADS=1 cargo bench --bench fxmark -- --duration 10 --type $benchmark
python3 scripts/plot.py $CSVFILE

# Unmount tmpfs
sudo umount -f /mnt

rm -rf gh-pages
git clone -b gh-pages git@vmops-gh-pages:gz/vmops-bench.git gh-pages

export GIT_REV_CURRENT=`git rev-parse --short HEAD`
export CSV_LINE="`date +%Y-%m-%d`",${GIT_REV_CURRENT},,"${CI_MACHINE_TYPE}/${GIT_REV_CURRENT}/index.html"
echo $CSV_LINE >> gh-pages/_data/$CI_MACHINE_TYPE.csv

DEPLOY_DIR="gh-pages/fsops/${CI_MACHINE_TYPE}/${GIT_REV_CURRENT}/"
mkdir -p ${DEPLOY_DIR}
cp gh-pages/fsops/index.markdown ${DEPLOY_DIR}

mv *.csv ${DEPLOY_DIR}
mv *.pdf ${DEPLOY_DIR}
mv *.png ${DEPLOY_DIR}

cd gh-pages
git add .
git commit -a -m "Added benchmark results for $GIT_REV_CURRENT."
git push origin gh-pages
cd ..
rm -rf gh-pages
git clean -f
