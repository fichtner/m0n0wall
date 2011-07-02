#!/usr/local/bin/bash

set -e

if [ -z "$MW_BUILDPATH" -o ! -d "$MW_BUILDPATH" ]; then
	echo "\$MW_BUILDPATH is not set"
	exit 1
fi

# set working directory for ports compilation
		rm -Rf $MW_BUILDPATH/tmp/ports/work
		mkdir -p $MW_BUILDPATH/tmp/ports/work
		export WRKDIRPREFIX=$MW_BUILDPATH/tmp/ports/work

# set port options for ports that need user input
		rm -Rf $MW_BUILDPATH/tmp/ports/db
		mkdir -p $MW_BUILDPATH/tmp/ports/db
		export PORT_DBDIR=$MW_BUILDPATH/tmp/ports/db
		
		for portoptf in $MW_BUILDPATH/freebsd8/build/files/portoptions/* ; do
			port=${portoptf##*/}
			mkdir -p $PORT_DBDIR/$port
			cp $portoptf $PORT_DBDIR/$port/options
		done


######## manually compiled packages ########

# select Autoconf version 2.13
		export AUTOCONF_VERSION=2.13
# php 4.4.9
		cd $MW_BUILDPATH/tmp
		rm -Rf php-4.4.9
        tar -zxf $MW_BUILDPATH/freebsd8/build/local-sources/php-4.4.9.tar.bz2
        cd php-4.4.9/ext/
		tar -zxf $MW_BUILDPATH/freebsd8/build/local-sources/radius-1.2.5.tgz
        mv radius-1.2.5 radius
        cd ..
        ./buildconf --force
        ./configure --without-mysql --with-pear --with-openssl --enable-discard-path --enable-radius --enable-sockets --enable-bcmath
        make
        install -s sapi/cgi/php $MW_BUILDPATH/m0n0fs/usr/local/bin/
# mini httpd
		cd $MW_BUILDPATH/tmp
		rm -Rf mini_httpd-1.19
        tar -zxf $MW_BUILDPATH/freebsd8/build/local-sources/mini_httpd-1.19.tar.gz
        cd mini_httpd-1.19/
        patch < $MW_BUILDPATH/freebsd8/build/patches/packages/mini_httpd.patch
        make
        install -s mini_httpd $MW_BUILDPATH/m0n0fs/usr/local/sbin
# ezipupdate
        cd $MW_BUILDPATH/tmp
		rm -Rf ez-ipupdate-3.0.11b8
        tar -zxf $MW_BUILDPATH/freebsd8/build/local-sources/ez-ipupdate-3.0.11b8.tar.gz
        cd ez-ipupdate-3.0.11b8
        patch < $MW_BUILDPATH/freebsd8/build/patches/packages/ez-ipupdate.c.patch
        ./configure
        make
        install -s ez-ipupdate $MW_BUILDPATH/m0n0fs/usr/local/bin/
# ipfilter userland tools (newer version than included with FreeBSD)
		cd $MW_BUILDPATH/tmp
		rm -Rf ip_fil4.1.34
        tar -zxf $MW_BUILDPATH/freebsd8/build/local-sources/ip_fil4.1.34.tar.gz
		cd ip_fil4.1.34
        patch < $MW_BUILDPATH/freebsd8/build/patches/user/ipfstat.c.patch
		make freebsd8
		install -s BSD/FreeBSD-8.?-RELEASE-$MW_ARCH/{ipf,ipfs,ipfstat,ipmon,ipnat} $MW_BUILDPATH/m0n0fs/sbin


######## FreeBSD ports ########

# ISC dhcp-relay
        cd /usr/ports/net/isc-dhcp31-relay
        make
        install -s $WRKDIRPREFIX/usr/ports/net/isc-dhcp31-relay/work/dhcp-*/work.freebsd/relay/dhcrelay $MW_BUILDPATH/m0n0fs/usr/local/sbin/
# ISC dhcp-server
        cd /usr/ports/net/isc-dhcp31-server
		cp $MW_BUILDPATH/freebsd8/build/patches/packages/isc-dhcpd/patch-server.db.c files/
        make
        install -s $WRKDIRPREFIX/usr/ports/net/isc-dhcp31-server/work/dhcp-*/work.freebsd/server/dhcpd $MW_BUILDPATH/m0n0fs/usr/local/sbin/
		rm files/patch-server.db.c
# ISC dhcp-client
		cd /usr/ports/net/isc-dhcp31-client
        make
		install -s $WRKDIRPREFIX/usr/ports/net/isc-dhcp31-client/work/dhcp-*/work.freebsd/client/dhclient $MW_BUILDPATH/m0n0fs/sbin/
# dnsmasq
        cd /usr/ports/dns/dnsmasq
        make
        install -s $WRKDIRPREFIX/usr/ports/dns/dnsmasq/work/dnsmasq-*/src/dnsmasq $MW_BUILDPATH/m0n0fs/usr/local/sbin/
# ipsec-tools
        cd /usr/ports/security/ipsec-tools
		patch < $MW_BUILDPATH/freebsd8/build/patches/packages/ipsec-tools.Makefile.patch
        make
        install -s $WRKDIRPREFIX/usr/ports/security/ipsec-tools/work/ipsec-tools-*/src/racoon/.libs/racoon $MW_BUILDPATH/m0n0fs/usr/local/sbin
        install -s $WRKDIRPREFIX/usr/ports/security/ipsec-tools/work/ipsec-tools-*/src/libipsec/.libs/libipsec.so.0 $MW_BUILDPATH/m0n0fs/usr/local/lib
		mv Makefile.orig Makefile
# dhcp6
		cd /usr/ports/net/dhcp6
        make
		install -s $WRKDIRPREFIX/usr/ports/net/dhcp6/work/wide-dhc*/dhcp6c $MW_BUILDPATH/m0n0fs/usr/local/sbin
		install -s $WRKDIRPREFIX/usr/ports/net/dhcp6/work/wide-dhc*/dhcp6s $MW_BUILDPATH/m0n0fs/usr/local/sbin
# sixxs-aiccu		
		cd /usr/ports/net/sixxs-aiccu
		cp -p Makefile Makefile.orig
		patch < $MW_BUILDPATH/freebsd8/build/patches/packages/sixxs-aiccu.Makefile.patch
        make
		install -s $WRKDIRPREFIX/usr/ports/net/sixxs-aiccu/work/aiccu/unix-console/aiccu $MW_BUILDPATH/m0n0fs/usr/local/sbin/sixxs-aiccu
		mv Makefile.orig Makefile
# mpd4 - XXX this will cause libpdel to be installed
		cd /usr/ports/net/mpd4
        make
		install -s $WRKDIRPREFIX/usr/ports/net/mpd4/work/mpd-*/src/mpd4 $MW_BUILDPATH/m0n0fs/usr/local/sbin/
# mbmon
		cd /usr/ports/sysutils/mbmon
        make
		install -s $WRKDIRPREFIX/usr/ports/sysutils/mbmon/work/xmbmon*/mbmon $MW_BUILDPATH/m0n0fs/usr/local/bin/
# wol
		cd /usr/ports/net/wol
		make WITHOUT_NLS=true
        install -s $WRKDIRPREFIX/usr/ports/net/wol/work/wol-*/src/wol $MW_BUILDPATH/m0n0fs/usr/local/bin/


# make m0n0wall tools and binaries
        cd $MW_BUILDPATH/tmp
        cp -r $MW_BUILDPATH/freebsd8/build/tools .
        cd tools
        gcc -o stats.cgi stats.c
        gcc -o minicron minicron.c
        gcc -o choparp choparp.c
        gcc -o verifysig -lcrypto verifysig.c
        gcc -o dnswatch dnswatch.c
        install -s choparp $MW_BUILDPATH/m0n0fs/usr/local/sbin
        install -s stats.cgi $MW_BUILDPATH/m0n0fs/usr/local/www
        install -s minicron $MW_BUILDPATH/m0n0fs//usr/local/bin
        install -s verifysig $MW_BUILDPATH/m0n0fs/usr/local/bin
        install -s dnswatch $MW_BUILDPATH/m0n0fs/usr/local/bin
        install runsntp.sh $MW_BUILDPATH/m0n0fs/usr/local/bin
        install ppp-linkup vpn-linkdown vpn-linkup $MW_BUILDPATH/m0n0fs/usr/local/sbin

# select Autoconf version 2.62
		export AUTOCONF_VERSION=2.62
# ucd-snmp
        cd $MW_BUILDPATH/tmp
		rm -Rf ucd-snmp-4.2.7.1
        tar -zxf $MW_BUILDPATH/freebsd8/build/local-sources/ucd-snmp-4.2.7.1.tar.gz
        cd ucd-snmp-4.2.7.1
        ./configure  --without-openssl --disable-debugging --enable-static \
        --enable-mini-agent --disable-privacy --disable-testing-code \
        --disable-shared-version --disable-shared \
        '--with-out-transports=TCP Unix' \
        '--with-mib-modules=mibII/interfaces mibII/var_route ucd-snmp/vmstat_freebsd2' \
		--with-defaults
		patch < $MW_BUILDPATH/freebsd8/build/patches/packages/ucd-snmp.patch
		patch < $MW_BUILDPATH/freebsd8/build/patches/packages/ucd-snmp.config.h.patch
	    make
        install -s agent/snmpd $MW_BUILDPATH/m0n0fs/usr/local/sbin

echo "Finished Stage 2"