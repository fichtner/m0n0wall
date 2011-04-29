#!/bin/csh

set -e

export MW_BUILDPATH=/usr/m0n0wall/build82

# ensure prerequisite tools are installed
if [ ! -x /usr/local/bin/bash ]; then
	pkg_add -r bash
fi
if [ ! -x /usr/local/bin/svn ]; then
	pkg_add -r subversion
fi

# figure out if we're already running from within a repository
if svn info > /dev/null 2>&1; then
	echo "Found existing working copy"
else
	echo "No working copy found; checking out current version from repository"
	svn checkout http://svn.m0n0.ch/wall/branches/freebsd8
	cd freebsd8/build/scripts
fi

echo "Creating build directory $MW_BUILDPATH."
mkdir -p $MW_BUILDPATH
cd ../..

echo "Exporting repository to $MW_BUILDPATH/freebsd8."
svn export . $MW_BUILDPATH/freebsd8

echo "Changing directory to $MW_BUILDPATH/freebsd8/build/scripts"
cd $MW_BUILDPATH/freebsd8/build/scripts
chmod +x *.sh

echo 
echo "----- Build environment prepared -----"
echo "I will leave you in a bash shell now"
echo "To start the build, execute doall.sh or run 1makebuildenv.sh , then 2makebinaries.sh, then 3patchtools.sh etc"
PS1="m0n0wall-build# " /usr/local/bin/bash
