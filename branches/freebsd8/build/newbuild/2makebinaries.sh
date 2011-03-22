#!/usr/local/bin/bash

# set port options for ports that need user input
		cd /usr/m0n0wall/build81/tmp
		tar -zxvf /usr/m0n0wall/build81/freebsd8/build/newbuild/files/portoptions.tgz -C /
 
# autconf
        rm /usr/local/bin/autoconf
        rm /usr/local/bin/autoheader
        ln -s /usr/local/bin/autoconf-2.13 /usr/local/bin/autoconf
        ln -s /usr/local/bin/autoheader-2.13 /usr/local/bin/autoheader
# php 4.4.9
        wget http://dk.php.net/distributions/php-4.4.9.tar.bz2
        tar -zxf php-4.4.9.tar.bz2
        cd php-4.4.9/ext/
        wget http://pecl.php.net/get/radius-1.2.5.tgz
        tar -zxf radius-1.2.5.tgz
        mv radius-1.2.5 radius
        cd ..
        ./buildconf --force
        ./configure --without-mysql --with-pear --with-openssl --enable-discard-path --enable-radius --enable-sockets --enable-bcmath
        make
        install -s sapi/cgi/php ../../m0n0fs/usr/local/bin/
        cd ..
# mini httpd
        wget http://acme.com/software/mini_httpd/mini_httpd-1.19.tar.gz
        tar -zxf mini_httpd-1.19.tar.gz
        rm mini_httpd-1.19.tar.gz
        cd mini_httpd-1.19/
        patch < ../../freebsd8/build/patches/packages/mini_httpd.patch
        make
        install -s mini_httpd ../../m0n0fs/usr/local/sbin
        cd ..
# wol
        cd /usr/m0n0wall/build81/tmp
        wget "http://downloads.sourceforge.net/project/wake-on-lan/wol/0.7.1/wol-0.7.1.tar.gz"
        tar -zxf wol-0.7.1.tar.gz
        cd wol-0.7.1
        ./configure --disable-nls
        make
        install -s src/wol /usr/m0n0wall/build81/m0n0fs/usr/local/bin/
# ezipupdate
        cd /usr/m0n0wall/build81/tmp
        wget http://dyn.pl/client/UNIX/ez-ipupdate/ez-ipupdate-3.0.11b8.tar.gz
        tar -zxf ez-ipupdate-3.0.11b8.tar.gz
        cd ez-ipupdate-3.0.11b8
        patch < ../../freebsd8/build/patches/packages/ez-ipupdate.c.patch
        ./configure
        make
        install -s ez-ipupdate /usr/m0n0wall/build81/m0n0fs/usr/local/bin/
        cd ..
# bpalogin
        wget "http://downloads.sourceforge.net/project/bpalogin/bpalogin/2.0.1/bpalogin-2.0.1-src.tar.gz"
        tar -zxf bpalogin-2.0.1-src.tar.gz
        cd bpalogin-2.0.1
        make
        install -s bpalogin /usr/m0n0wall/build81/m0n0fs/usr/local/bin/
# ISC dhcp-relay
        cd /usr/ports/net/isc-dhcp31-relay
        make
        install -s work/dhcp-*/work.freebsd/relay/dhcrelay /usr/m0n0wall/build81/m0n0fs/usr/local/sbin/
# ISC dhcp-server
        cd /usr/ports/net/isc-dhcp31-server
		cp /usr/m0n0wall/build81/freebsd8/build/patches/packages/isc-dhcpd/patch-server.db.c /usr/ports/net/isc-dhcp31-server/files/
        make
        install -s work/dhcp-*/work.freebsd/server/dhcpd /usr/m0n0wall/build81/m0n0fs/usr/local/sbin/
# ISC dhcp-client
		cd /usr/ports/net/isc-dhcp31-client
		make
		install -s work/dhcp-*/work.freebsd/client/dhclient /usr/m0n0wall/build81/m0n0fs/sbin/
# dnsmasq
        cd /usr/ports/dns/dnsmasq
		make
        install -s work/dnsmasq-*/src/dnsmasq /usr/m0n0wall/build81/m0n0fs/usr/local/sbin/
# ipsec-tools
        cd /usr/ports/security/ipsec-tools
		patch < /usr/m0n0wall/build81/freebsd8/build/newbuild/patches/ipsec-tools.Makefile.patch
        make
        install -s work/ipsec-tools-*/src/racoon/.libs/racoon /usr/m0n0wall/build81/m0n0fs/usr/local/sbin
        install -s work/ipsec-tools-*/src/libipsec/.libs/libipsec.so.0 /usr/m0n0wall/build81/m0n0fs/usr/local/lib
		install work/ipsec-tools-*/src/setkey/setkey /usr/m0n0wall/build81/m0n0fs/sbin

# dhcp6
		cd /usr/ports/net/dhcp6
		make
		install -s work/wide-dhc*/dhcp6c /usr/m0n0wall/build81/m0n0fs/usr/local/sbin
		install -s work/wide-dhc*/dhcp6s /usr/m0n0wall/build81/m0n0fs/usr/local/sbin
# sixxs-aiccu		
		cd /usr/ports/net/sixxs-aiccu/
		make
		install -s work/aiccu/unix-console/aiccu /usr/m0n0wall/build81/m0n0fs/usr/local/sbin/sixxs-aiccu
# rtadvd		
		cd /usr/src/usr.sbin/rtadvd
		make
		install -s rtadvd /usr/m0n0wall/build81/m0n0fs/usr/sbin/
# mpd4
		cd /usr/ports/net/mpd4
		make
		install -s work/mpd-4.4.1/src/mpd4 /usr/m0n0wall/build81/m0n0fs/usr/local/sbin/
# mbmon
		cd /usr/ports/sysutils/mbmon
		make
		install -s work/xmbmon205/mbmon /usr/m0n0wall/build81/m0n0fs/usr/local/bin/

# make m0n0wall tools and binaries
        cd /usr/m0n0wall/build81/tmp
        cp -r ../freebsd8/build/tools .
        cd tools
        gcc -o stats.cgi stats.c
        gcc -o minicron minicron.c
        # gcc -o atareinit atareinit.c
        gcc -o choparp choparp.c
        gcc -o verifysig -lcrypto verifysig.c
        gcc -o dnswatch dnswatch.c
        install -s choparp /usr/m0n0wall/build81/m0n0fs/usr/local/sbin
        install -s stats.cgi /usr/m0n0wall/build81/m0n0fs/usr/local/www
        install -s minicron /usr/m0n0wall/build81/m0n0fs//usr/local/bin
        install -s verifysig /usr/m0n0wall/build81/m0n0fs/usr/local/bin
        install -s dnswatch /usr/m0n0wall/build81/m0n0fs/usr/local/bin
        install runsntp.sh /usr/m0n0wall/build81/m0n0fs/usr/local/bin
        install ppp-linkup vpn-linkdown vpn-linkup /usr/m0n0wall/build81/m0n0fs/usr/local/sbin

# ucd-snmp
        rm /usr/local/bin/autoconf
        rm /usr/local/bin/autoheader
        ln -s /usr/local/bin/autoconf-2.62 /usr/local/bin/autoconf
        ln -s /usr/local/bin/autoheader-2.62 /usr/local/bin/autoheader
        cd /usr/m0n0wall/build81/tmp
        wget "http://downloads.sourceforge.net/project/net-snmp/ucd-snmp/4.2.7.1/ucd-snmp-4.2.7.1.tar.gz"
        tar -zxf ucd-snmp-4.2.7.1.tar.gz
        cd ucd-snmp-4.2.7.1
        ./configure  --without-openssl --disable-debugging --enable-static \
        --enable-mini-agent --disable-privacy --disable-testing-code \
        --disable-shared-version --disable-shared \
        '--with-out-transports=TCP Unix' \
        '--with-mib-modules=mibII/interfaces mibII/var_route ucd-snmp/vmstat_freebsd2' \
		--with-defaults
		patch < /usr/m0n0wall/build81/freebsd8/build/newbuild/patches/ucd-snmp.config.h.patch
	    make
        install -s agent/snmpd /usr/m0n0wall/build81/m0n0fs/usr/local/sbin
