#!/usr/local/bin/bash

# patch kernel / sources
		cd /usr/src
		wget http://svn.m0n0.ch/wall/branches/freebsd8/build/newbuild/kernelpatches/Makefile.orig.patch
		patch < Makefile.orig.patch
		wget http://svn.m0n0.ch/wall/branches/freebsd8/build/newbuild/kernelpatches/options.orig.patch
		patch < options.orig.patch
		wget http://svn.m0n0.ch/wall/branches/freebsd8/build/newbuild/kernelpatches/ip_ftp_pxy.c.orig.patch
		patch < ip_ftp_pxy.c.orig.patch
		wget http://svn.m0n0.ch/wall/branches/freebsd8/build/newbuild/kernelpatches/ip_nat.c.orig.patch
		patch < ip_nat.c.orig.patch
		# wget http://svn.m0n0.ch/wall/branches/freebsd8/build/newbuild/kernelpatches/ip_input.c.orig.patch 
		# patch < ip_input.c.orig.patch
		get http://svn.m0n0.ch/wall/branches/freebsd8/build/newbuild/kernelpatches/fil.c.orig.patch 
		patch < fil.c.orig.patch
		wget http://svn.m0n0.ch/wall/branches/freebsd8/build/newbuild/kernelpatches/ip_state.c.orig.patch
		patch < ip_state.c.orig.patch
		wget http://svn.m0n0.ch/wall/branches/freebsd8/build/newbuild/kernelpatches/mlfk_ipl.c.orig.patch 
		patch < mlfk_ipl.c.orig.patch
		wget http://svn.m0n0.ch/wall/branches/freebsd8/build/newbuild/kernelpatches/pfil.c.orig.patch 
		patch < pfil.c.orig.patch 
		wget http://svn.m0n0.ch/wall/branches/freebsd8/build/newbuild/kernelpatches/vm_machdep.c.orig.patch 
		patch < vm_machdep.c.orig.patch 


# kernel compile	got to remove rg lnc ugen ADAPTIVE_GIANT and sio
#					rename FAST_IPSEC to IPSEC
#					add COMPAT_FREEBSD7 AH_SUPPORT_AR5416 and COMPAT43_TTYS
        cd /sys/i386/conf
        cp /usr/m0n0wall/build81/freebsd8/build/kernelconfigs/M0N0WALL_GENERIC* /sys/i386/conf/
        wget http://svn.m0n0.ch/wall/branches/freebsd8/build/newbuild/patches/M0N0WALL_GENERIC.patch
		patch < M0N0WALL_GENERIC.patch
        config M0N0WALL_GENERIC
        cd /sys/i386/compile/M0N0WALL_GENERIC/
        make depend && make
        strip kernel
        strip --remove-section=.note --remove-section=.comment kernel
        gzip -9 kernel
        mv kernel.gz /usr/m0n0wall/build81/tmp/
        cd modules/usr/src/sys/modules
        cp /boot/kernel/if_tap.ko /boot/kernel/if_vlan.ko dummynet/dummynet.ko ipfw/ipfw.ko /usr/m0n0wall/build81/m0n0fs/boot/kernel
		cp /boot/kernel/acpi.ko /usr/m0n0wall/build81/tmp

# make libs
		cd /usr/m0n0wall/build81/tmp
		perl /usr/m0n0wall/build81/freebsd8/build/minibsd/mklibs.pl /usr/m0n0wall/build81/m0n0fs > m0n0wall.libs
		perl /usr/m0n0wall/build81/freebsd8/build/minibsd/mkmini.pl m0n0wall.libs / /usr/m0n0wall/build81/m0n0fs
