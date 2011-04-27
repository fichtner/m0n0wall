#!/usr/local/bin/bash

set -e

# patch kernel / sources
		cd /usr/src
		patch < /usr/m0n0wall/build82/freebsd8/build/newbuild/kernelpatches/ipfilter_kernel_update_to_4.1.34.patch
		patch < /usr/m0n0wall/build82/freebsd8/build/newbuild/kernelpatches/Makefile.orig.patch
		patch < /usr/m0n0wall/build82/freebsd8/build/newbuild/kernelpatches/options.orig.patch
		patch < /usr/m0n0wall/build82/freebsd8/build/newbuild/kernelpatches/ip_ftp_pxy.c.orig.patch
		patch < /usr/m0n0wall/build82/freebsd8/build/newbuild/kernelpatches/ip_nat.c.orig.patch
		# patch < /usr/m0n0wall/build82/freebsd8/build/newbuild/kernelpatches/ip_input.c.orig.patch
		patch < /usr/m0n0wall/build82/freebsd8/build/newbuild/kernelpatches/fil.c.orig.patch
		patch < /usr/m0n0wall/build82/freebsd8/build/newbuild/kernelpatches/ip_state.c.orig.patch
		patch < /usr/m0n0wall/build82/freebsd8/build/newbuild/kernelpatches/mlfk_ipl.c.orig.patch
		patch < /usr/m0n0wall/build82/freebsd8/build/newbuild/kernelpatches/pfil.c.orig.patch 
		patch < /usr/m0n0wall/build82/freebsd8/build/newbuild/kernelpatches/g_part_bsd.c.orig.patch


# kernel compile	got to remove rg lnc ugen ADAPTIVE_GIANT and sio
#					rename FAST_IPSEC to IPSEC
#					add COMPAT_FREEBSD7 AH_SUPPORT_AR5416 and COMPAT43_TTYS
        cd /sys/i386/conf
        cp /usr/m0n0wall/build82/freebsd8/build/kernelconfigs/M0N0WALL_GENERIC* /sys/i386/conf/
        config M0N0WALL_GENERIC
        cd /sys/i386/compile/M0N0WALL_GENERIC/
        make depend && make
        strip kernel
        strip --remove-section=.note --remove-section=.comment kernel
        gzip -9 kernel
        mv kernel.gz /usr/m0n0wall/build82/tmp/
        cd modules/usr/src/sys/modules
        cp /boot/kernel/if_tap.ko /boot/kernel/if_vlan.ko dummynet/dummynet.ko ipfw/ipfw.ko /usr/m0n0wall/build82/m0n0fs/boot/kernel
		cp /boot/kernel/acpi.ko /usr/m0n0wall/build82/tmp

# make libs
		cd /usr/m0n0wall/build82/tmp
		perl /usr/m0n0wall/build82/freebsd8/build/minibsd/mklibs.pl /usr/m0n0wall/build82/m0n0fs > m0n0wall.libs
		perl /usr/m0n0wall/build82/freebsd8/build/minibsd/mkmini.pl m0n0wall.libs / /usr/m0n0wall/build82/m0n0fs
