#!/bin/csh

set -e

if [ ! -x /usr/local/bin/bash ]; then
	pkg_add -r bash
fi
mkdir -p /usr/m0n0wall/build82
cp doall.sh 1makebuildenv.sh 2makebinaries.sh 3patchtools.sh 4buildkernel.sh 5makeimage.sh /usr/m0n0wall/build82
cd ../..
svn export . /usr/m0n0wall/build82/freebsd8
cd  /usr/m0n0wall/build82
chmod a+rx *.sh

echo 
echo "----- Build environment prepared -----"
echo "I will leave you in a bash shell now"
echo "To start the build, execute doall.sh or run 1makebuildenv.sh , then 2makebinaries.sh, then 3patchtools.sh etc"
PS1="m0n0wall-build# " /usr/local/bin/bash
