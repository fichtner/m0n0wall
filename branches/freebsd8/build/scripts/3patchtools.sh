#!/usr/local/bin/bash

set -e

if [ -z "$MW_BUILDPATH" -o ! -d "$MW_BUILDPATH" ]; then
	echo "\$MW_BUILDPATH is not set"
	exit 1
fi

# syslogd circular logging support and ipv6 support
		rm -Rf /usr/obj/usr/src/usr.sbin/syslogd
		rm -Rf /usr/obj/usr/src/usr.sbin/clog
		cd /usr/src/usr.sbin
		tar xfvz $MW_BUILDPATH/freebsd8/build/patches/user/clog-1.0.1.tar.gz
        cd syslogd
        patch < $MW_BUILDPATH/freebsd8/build/patches/user/syslogd.c.patch
        make obj && make
        install -s /usr/obj/usr/src/usr.sbin/syslogd/syslogd $MW_BUILDPATH/m0n0fs/usr/sbin/
		mv syslogd.c.orig syslogd.c
		cd ../clog
        make obj && make
        install -s /usr/obj/usr/src/usr.sbin/clog/clog $MW_BUILDPATH/m0n0fs/usr/sbin/
		cd ..
		rm -Rf clog
# dhclient-script
        cp $MW_BUILDPATH/freebsd8/build/tools/dhclient-script $MW_BUILDPATH/m0n0fs/sbin
        chmod a+rx $MW_BUILDPATH/m0n0fs/sbin/dhclient-script
# setkey bin/147887 patch
# XXX this modifies the host system's copy of libipsec, but this is
# needed as we later copy all libraries from the base system
		rm -Rf /usr/obj/usr/src/lib/libipsec
		rm -Rf /usr/obj/usr/src/sbin/setkey
		cd /usr/src/lib/libipsec
		patch < $MW_BUILDPATH/freebsd8/build/patches/user/pfkey.c.patch
		make obj && make && make install
		cd /usr/src/sbin/setkey
		make obj && make
		install -s /usr/obj/usr/src/sbin/setkey/setkey $MW_BUILDPATH/m0n0fs/sbin/
		cd /usr/src/lib/libipsec
		mv pfkey.c.orig pfkey.c
# lets strip out any missed symbols lazy way , lots of harmless errors to dev null
	set +e
	find $MW_BUILDPATH/m0n0fs/ | xargs strip -s 2> /dev/null
	echo "Finished Stage 3"
