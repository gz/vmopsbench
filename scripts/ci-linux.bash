#!/bin/bash
#
# Usage:
# - CI_MACHINE_TYPE=skylake2x bash scripts/ci-linux.bash latency
# - CI_MACHINE_TYPE=skylake2x bash scripts/ci-linux.bash throughput
#

BASE="$(dirname "$0")"
source "$BASE/linux.bash"

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

