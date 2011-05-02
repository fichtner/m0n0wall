#!/usr/local/bin/bash

set -e

if [ -z "$MW_BUILDPATH" -o ! -d "$MW_BUILDPATH" ]; then
	echo "\$MW_BUILDPATH is not set"
	exit 1
fi

# syslogd circular logging support and ipv6 support
        cd /usr/src/usr.sbin/syslogd
        patch < $MW_BUILDPATH/freebsd8/build/patches/user/syslogd.c.ipv6.patch
        cd /usr/src
	patch < $MW_BUILDPATH/freebsd8/build/patches/user/syslogd.c.patch
        cd usr.sbin
        tar xfvz $MW_BUILDPATH/freebsd8/build/patches/user/clog-1.0.1.tar.gz
        cd syslogd
        make
        install -s /usr/src/usr.sbin/syslogd/syslogd $MW_BUILDPATH/m0n0fs/usr/sbin/
        cd ../clog
        make obj && make
        install -s /usr/obj/usr/src/usr.sbin/clog/clog $MW_BUILDPATH/m0n0fs/usr/sbin/
# dhclient-script
        cp $MW_BUILDPATH/freebsd8/build/tools/dhclient-script $MW_BUILDPATH/m0n0fs/sbin
        chmod a+rx $MW_BUILDPATH/m0n0fs/sbin/dhclient-script

# lets strip out any missed symbols lazy way , lots of harmless errors to dev null
set +e
	find $MW_BUILDPATH/m0n0fs/ | xargs strip -s 2> /dev/null
