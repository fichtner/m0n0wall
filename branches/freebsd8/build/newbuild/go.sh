#!/bin/csh

pkg_add -r wget bash
rehash
mkdir -p /usr/m0n0wall/build81
cd  /usr/m0n0wall/build81
wget http://svn.m0n0.ch/wall/branches/freebsd8/build/newbuild/doall.sh
wget http://svn.m0n0.ch/wall/branches/freebsd8/build/newbuild/1makebuildenv.sh
wget http://svn.m0n0.ch/wall/branches/freebsd8/build/newbuild/2makebinaries.sh
wget http://svn.m0n0.ch/wall/branches/freebsd8/build/newbuild/3patchtools.sh
wget http://svn.m0n0.ch/wall/branches/freebsd8/build/newbuild/4buildkernel.sh
wget http://svn.m0n0.ch/wall/branches/freebsd8/build/newbuild/5makeimage.sh
chmod a+rx *.sh

echo "I will leave you in a bash shell now"
echo "execute doall.sh or run 1makebuildenv.sh , then 2makebinaries.sh, then 3patchtools.sh etc"
/usr/local/bin/bash

