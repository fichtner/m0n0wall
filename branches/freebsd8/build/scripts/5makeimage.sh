#!/usr/local/bin/bash

set -e

if [ ! -d $MW_BUILDPATH ]; then
	echo "\$MW_BUILDPATH is not set"
	exit 1
fi

# make bootloader
	cd /usr/src
	patch < $MW_BUILDPATH/freebsd8/build/patches/boot/bootinfo.c.patch 
	cd /sys/boot
	make clean && make obj && make
	mkdir $MW_BUILDPATH/tmp/bootdir
	cp /usr/obj/usr/src/sys/boot/i386/loader/loader $MW_BUILDPATH/tmp/bootdir
	cp /usr/obj/usr/src/sys/boot/i386/boot2/{boot,boot1,boot2} $MW_BUILDPATH/tmp/bootdir
	cp /usr/obj/usr/src/sys/boot/i386/cdboot/cdboot $MW_BUILDPATH/tmp/bootdir
	cp $MW_BUILDPATH/freebsd8/build/boot/generic-pc/loader.rc $MW_BUILDPATH/tmp/bootdir

# Creating mfsroot with 4MB spare space
	echo "generic-pc-cdrom" > $MW_BUILDPATH/m0n0fs/etc/platform
	cd $MW_BUILDPATH/tmp
	dd if=/dev/zero of=mfsroot bs=1k count=`du -d0 $MW_BUILDPATH/m0n0fs | cut -b1-5 | tr " " "+" | xargs -I {} echo '(4096)+{}' | bc`
	mdconfig -a -t vnode -f $MW_BUILDPATH/tmp/mfsroot -u 20
	disklabel -rw /dev/md20 auto
	newfs -b 8192 -f 1024 -o space -m 0 /dev/md20
	mount /dev/md20 /mnt
	cd /mnt
	tar -cf - -C $MW_BUILDPATH/m0n0fs ./ | tar -xvpf -
	cd $MW_BUILDPATH/tmp
	umount /mnt
	mdconfig -d -u 20
	gzip -9f mfsroot

# Creating generic-pc mfsroot with 4MB spare space
        echo "generic-pc" > $MW_BUILDPATH/m0n0fs/etc/platform
        cd $MW_BUILDPATH/tmp
        dd if=/dev/zero of=mfsrootpc bs=1k count=`du -d0 $MW_BUILDPATH/m0n0fs | cut -b1-5 | tr " " "+" | xargs -I {} echo '(4096)+{}' | bc`
        mdconfig -a -t vnode -f $MW_BUILDPATH/tmp/mfsrootpc -u 20
        disklabel -rw /dev/md20 auto
        newfs -b 8192 -f 1024 -o space -m 0 /dev/md20
        mount /dev/md20 /mnt
        cd /mnt
        tar -cf - -C $MW_BUILDPATH/m0n0fs ./ | tar -xvpf -
        cd $MW_BUILDPATH/tmp
        umount /mnt
        mdconfig -d -u 20
        gzip -9f mfsrootpc
	
# Make firmware img with 2MB space
	# Make staging area to help calc space
	mkdir $MW_BUILDPATH/tmp/firmwaretmp
 	
	cp $MW_BUILDPATH/tmp/kernel.gz $MW_BUILDPATH/tmp/firmwaretmp
	cp $MW_BUILDPATH/tmp/mfsrootpc.gz $MW_BUILDPATH/tmp/firmwaretmp/mfsroot.gz
	cp $MW_BUILDPATH/tmp/bootdir/{loader,loader.rc} $MW_BUILDPATH/tmp/firmwaretmp
	cp $MW_BUILDPATH/tmp/acpi.ko $MW_BUILDPATH/tmp/firmwaretmp
	cp $MW_BUILDPATH/m0n0fs/conf.default/config.xml $MW_BUILDPATH/tmp/firmwaretmp

	cd $MW_BUILDPATH/tmp
	dd if=/dev/zero of=image.bin bs=1k count=`du -d0 $MW_BUILDPATH/tmp/firmwaretmp  | cut -b1-5 | tr " " "+" | xargs -I {} echo '(2048)+{}' | bc`
	rm -rf $MW_BUILDPATH/tmp/firmwaretmp
	
	mdconfig -a -t vnode -f $MW_BUILDPATH/tmp/image.bin -u 30
	disklabel  -wn  /dev/md30 auto |  awk '/unused/{if (M==""){sub("unused","4.2BSD");M=1}}{print}' > md.label
        bsdlabel -m  i386 -R -B -b $MW_BUILDPATH/tmp/bootdir/boot /dev/md30 md.label
        newfs -b 8192 -f 1024 -O2 -U -o space -m 0 /dev/md30a
	mount /dev/md30a /mnt
	
	cp $MW_BUILDPATH/tmp/kernel.gz /mnt/
	cp $MW_BUILDPATH/tmp/mfsrootpc.gz /mnt/mfsroot.gz
	mkdir -p /mnt/boot/kernel
	cp $MW_BUILDPATH/tmp/bootdir/{loader,loader.rc} /mnt/boot
	cp $MW_BUILDPATH/tmp/acpi.ko /mnt/boot/kernel
	mkdir /mnt/conf
	cp $MW_BUILDPATH/m0n0fs/conf.default/config.xml /mnt/conf
	cd $MW_BUILDPATH/tmp
	umount /mnt
	mdconfig -d -u 30
	gzip -9f image.bin
	mkdir -p $MW_BUILDPATH/tmp/cdroot/boot/
	cp image.bin.gz $MW_BUILDPATH/tmp/cdroot/firmware.img
	mv image.bin.gz $MW_BUILDPATH/images/generic-pc-1.8.0.img
	 
	 
# Make ISO
	cd $MW_BUILDPATH/tmp
	mkdir -p $MW_BUILDPATH/tmp/cdroot/boot/kernel
	cp $MW_BUILDPATH/tmp/acpi.ko $MW_BUILDPATH/tmp/cdroot/boot/kernel
	cp $MW_BUILDPATH/tmp/bootdir/{cdboot,loader,loader.rc} $MW_BUILDPATH/tmp/cdroot/boot
	cp kernel.gz mfsroot.gz $MW_BUILDPATH/tmp/cdroot/ 
	mkisofs -b "boot/cdboot" -no-emul-boot -A "m0n0wall 1.8.0 CD-ROM image" \
        -c "boot/boot.catalog" -d -r -publisher "m0n0.ch" \
        -p "m0n0.ch" -V "m0n0wall_cd" -o "m0n0wall.iso" \
        $MW_BUILDPATH/tmp/cdroot
	mv m0n0wall.iso $MW_BUILDPATH/images/generic-pc-1.8.0.iso
