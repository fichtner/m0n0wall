#!/usr/local/bin/bash

set -e

if [ -z "$MW_BUILDPATH" -o ! -d "$MW_BUILDPATH" ]; then
	echo "\$MW_BUILDPATH is not set"
	exit 1
fi

# set port options for ports that need user input
		for portoptf in $MW_BUILDPATH/freebsd8/build/files/portoptions/* ; do
			port=${portoptf##*/}
			mkdir -p /var/db/ports/$port
			cp $portoptf /var/db/ports/$port/options
		done

# autconf
        rm /usr/local/bin/autoconf
        rm /usr/local/bin/autoheader
        ln -s /usr/local/bin/autoconf-2.13 /usr/local/bin/autoconf
        ln -s /usr/local/bin/autoheader-2.13 /usr/local/bin/autoheader
# php 4.4.9
		cd $MW_BUILDPATH/tmp
        fetch http://dk.php.net/distributions/php-4.4.9.tar.bz2
        tar -zxf php-4.4.9.tar.bz2
        cd php-4.4.9/ext/
        fetch http://pecl.php.net/get/radius-1.2.5.tgz
        tar -zxf radius-1.2.5.tgz
        mv radius-1.2.5 radius
        cd ..
        ./buildconf --force
        ./configure --without-mysql --with-pear --with-openssl --enable-discard-path --enable-radius --enable-sockets --enable-bcmath
        make
        install -s sapi/cgi/php $MW_BUILDPATH/m0n0fs/usr/local/bin/
        cd ..
# mini httpd
		cd $MW_BUILDPATH/tmp
        fetch http://acme.com/software/mini_httpd/mini_httpd-1.19.tar.gz
        tar -zxf mini_httpd-1.19.tar.gz
        rm mini_httpd-1.19.tar.gz
        cd mini_httpd-1.19/
        patch < $MW_BUILDPATH/freebsd8/build/patches/packages/mini_httpd.patch
        make
        install -s mini_httpd $MW_BUILDPATH/m0n0fs/usr/local/sbin
        cd ..
# wol
		cd /usr/ports/net/wol
		make WITHOUT_NLS=true
        install -s work/wol-*/src/wol $MW_BUILDPATH/m0n0fs/usr/local/bin/
# ezipupdate
        cd $MW_BUILDPATH/tmp
        fetch http://dyn.pl/client/UNIX/ez-ipupdate/ez-ipupdate-3.0.11b8.tar.gz
        tar -zxf ez-ipupdate-3.0.11b8.tar.gz
        cd ez-ipupdate-3.0.11b8
        patch < $MW_BUILDPATH/freebsd8/build/patches/packages/ez-ipupdate.c.patch
        ./configure
        make
        install -s ez-ipupdate $MW_BUILDPATH/m0n0fs/usr/local/bin/
        cd ..
# ipfilter userland tools (newer version than included with FreeBSD)
		cd $MW_BUILDPATH/tmp
		tar -zxvf $MW_BUILDPATH/freebsd8/build/local-sources/ip_fil4.1.34.tar.gz
		cd ip_fil4.1.34
        patch < $MW_BUILDPATH/freebsd8/build/patches/user/ipfstat.c.patch
		make freebsd8
		install -s BSD/FreeBSD-8.?-RELEASE-$MW_ARCH/{ipf,ipfs,ipfstat,ipmon,ipnat} $MW_BUILDPATH/m0n0fs/sbin
# ISC dhcp-relay
        cd /usr/ports/net/isc-dhcp31-relay
        make
        install -s work/dhcp-*/work.freebsd/relay/dhcrelay $MW_BUILDPATH/m0n0fs/usr/local/sbin/
# ISC dhcp-server
        cd /usr/ports/net/isc-dhcp31-server
		cp $MW_BUILDPATH/freebsd8/build/patches/packages/isc-dhcpd/patch-server.db.c /usr/ports/net/isc-dhcp31-server/files/
        make
        install -s work/dhcp-*/work.freebsd/server/dhcpd $MW_BUILDPATH/m0n0fs/usr/local/sbin/
# ISC dhcp-client
		cd /usr/ports/net/isc-dhcp31-client
		make
		install -s work/dhcp-*/work.freebsd/client/dhclient $MW_BUILDPATH/m0n0fs/sbin/
# dnsmasq
        cd /usr/ports/dns/dnsmasq
		make
        install -s work/dnsmasq-*/src/dnsmasq $MW_BUILDPATH/m0n0fs/usr/local/sbin/
# ipsec-tools
        cd /usr/ports/security/ipsec-tools
		patch < $MW_BUILDPATH/freebsd8/build/patches/packages/ipsec-tools.Makefile.patch
        make
        install -s work/ipsec-tools-*/src/racoon/.libs/racoon $MW_BUILDPATH/m0n0fs/usr/local/sbin
        install -s work/ipsec-tools-*/src/libipsec/.libs/libipsec.so.0 $MW_BUILDPATH/m0n0fs/usr/local/lib
# dhcp6
		cd /usr/ports/net/dhcp6
		make
		install -s work/wide-dhc*/dhcp6c $MW_BUILDPATH/m0n0fs/usr/local/sbin
		install -s work/wide-dhc*/dhcp6s $MW_BUILDPATH/m0n0fs/usr/local/sbin
# sixxs-aiccu		
		cd /usr/ports/net/sixxs-aiccu/
		patch < $MW_BUILDPATH/freebsd8/build/patches/packages/sixxs-aiccu.Makefile.patch
		make
		install -s work/aiccu/unix-console/aiccu $MW_BUILDPATH/m0n0fs/usr/local/sbin/sixxs-aiccu
# rtadvd		
		cd /usr/src/usr.sbin/rtadvd
		make
		install -s rtadvd $MW_BUILDPATH/m0n0fs/usr/sbin/
# mpd4
		cd /usr/ports/net/mpd4
		make
		install -s work/mpd-4.4.1/src/mpd4 $MW_BUILDPATH/m0n0fs/usr/local/sbin/
# mbmon
		cd /usr/ports/sysutils/mbmon
		make
		install -s work/xmbmon205/mbmon $MW_BUILDPATH/m0n0fs/usr/local/bin/

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

# ucd-snmp
        rm /usr/local/bin/autoconf
        rm /usr/local/bin/autoheader
        ln -s /usr/local/bin/autoconf-2.62 /usr/local/bin/autoconf
        ln -s /usr/local/bin/autoheader-2.62 /usr/local/bin/autoheader
        cd $MW_BUILDPATH/tmp
        fetch "http://downloads.sourceforge.net/project/net-snmp/ucd-snmp/4.2.7.1/ucd-snmp-4.2.7.1.tar.gz"
        tar -zxf ucd-snmp-4.2.7.1.tar.gz
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