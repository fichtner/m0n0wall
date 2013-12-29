#!/usr/local/bin/bash

set -e

if [ -z "$MW_BUILDPATH" -o ! -d "$MW_BUILDPATH" ]; then
	echo "\$MW_BUILDPATH is not set"
	exit 1
fi

export CC=gcc46

# syslogd circular logging support and ipv6 support
	rm -Rf /usr/obj/usr/src/usr.sbin/syslogd
	rm -Rf /usr/obj/usr/src/usr.sbin/clog
	cd /usr/src/usr.sbin
	tar xfvz $MW_BUILDPATH/freebsd10/build/patches/user/clog-1.0.1.tar.gz
	cd syslogd
	patch < $MW_BUILDPATH/freebsd10/build/patches/user/syslogd.c.patch
	make obj && make
	install -s /usr/obj/usr/src/usr.sbin/syslogd/syslogd $MW_BUILDPATH/m0n0fs/usr/sbin/
	mv syslogd.c.orig syslogd.c
	cd ../clog
	make obj && make
	install -s /usr/obj/usr/src/usr.sbin/clog/clog $MW_BUILDPATH/m0n0fs/usr/sbin/
	cd ..
	rm -Rf clog
# dhclient-script
	cp $MW_BUILDPATH/freebsd10/build/tools/dhclient-script $MW_BUILDPATH/m0n0fs/sbin
	chmod a+rx $MW_BUILDPATH/m0n0fs/sbin/dhclient-script
# rtadvd remove logging for dhcp-pd
	rm -Rf /usr/obj/usr/src/usr.sbin/rtadvd
	cd /usr/src/usr.sbin/rtadvd
	patch < $MW_BUILDPATH/freebsd10/build/patches/user/rtadvd.dhcppd.patch
	# make with patched libraries or the patch above won't work
	make obj && make CFLAGS='-I $MW_BUILDPATH/m0n0fs/tmp' 
	install -s /usr/obj/usr/src/usr.sbin/rtadvd/rtadvd $MW_BUILDPATH/m0n0fs/usr/sbin/
	mv rtadvd.c.orig rtadvd.c
# lets strip out any missed symbols lazy way , lots of harmless errors to dev null
	set +e
	find $MW_BUILDPATH/m0n0fs/ | xargs strip -s 2> /dev/null
	echo "Finished Stage 3"
