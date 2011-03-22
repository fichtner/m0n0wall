#!/usr/local/bin/bash

# make bootloader
	cd /usr/src
	patch < /usr/m0n0wall/build81/freebsd8/build/patches/boot/bootinfo.c.patch 
	cd /sys/boot
	make clean && make obj && make
	mkdir /usr/m0n0wall/build81/tmp/bootdir
	cp /usr/obj/usr/src/sys/boot/i386/loader/loader /usr/m0n0wall/build81/tmp/bootdir
	cp /usr/obj/usr/src/sys/boot/i386/boot2/{boot,boot1,boot2} /usr/m0n0wall/build81/tmp/bootdir
	cp /usr/obj/usr/src/sys/boot/i386/cdboot/cdboot /usr/m0n0wall/build81/tmp/bootdir
	wget http://m0n0wall2.googlecode.com/svn/trunk/loader.rc
	mv loader.rc /usr/m0n0wall/build81/tmp/bootdir

# Creating mfsroot with 4MB spare space
	echo "generic-pc-cdrom" > /usr/m0n0wall/build81/m0n0fs/etc/platform
	cd /usr/m0n0wall/build81/tmp
	dd if=/dev/zero of=mfsroot bs=1k count=`du -d0 /usr/m0n0wall/build81/m0n0fs | cut -b1-5 | tr " " "+" | xargs -I {} echo '(4096)+{}' | bc`
	mdconfig -a -t vnode -f /usr/m0n0wall/build81/tmp/mfsroot -u 20
	disklabel -rw /dev/md20 auto
	newfs -b 8192 -f 1024 -o space -m 0 /dev/md20
	mount /dev/md20 /mnt
	cd /mnt
	tar -cf - -C /usr/m0n0wall/build81/m0n0fs ./ | tar -xvpf -
	cd /usr/m0n0wall/build81/tmp
	umount /mnt
	mdconfig -d -u 20
	gzip -9f mfsroot

# Creating generic-pc mfsroot with 4MB spare space
        echo "generic-pc" > /usr/m0n0wall/build81/m0n0fs/etc/platform
        cd /usr/m0n0wall/build81/tmp
        dd if=/dev/zero of=mfsrootpc bs=1k count=`du -d0 /usr/m0n0wall/build81/m0n0fs | cut -b1-5 | tr " " "+" | xargs -I {} echo '(4096)+{}' | bc`
        mdconfig -a -t vnode -f /usr/m0n0wall/build81/tmp/mfsrootpc -u 20
        disklabel -rw /dev/md20 auto
        newfs -b 8192 -f 1024 -o space -m 0 /dev/md20
        mount /dev/md20 /mnt
        cd /mnt
        tar -cf - -C /usr/m0n0wall/build81/m0n0fs ./ | tar -xvpf -
        cd /usr/m0n0wall/build81/tmp
        umount /mnt
        mdconfig -d -u 20
        gzip -9f mfsrootpc
	
# Make firmware img with 2MB space
	# Make staging area to help calc space
	mkdir /usr/m0n0wall/build81/tmp/firmwaretmp
 	
	cp /usr/m0n0wall/build81/tmp/kernel.gz /usr/m0n0wall/build81/tmp/firmwaretmp
	cp /usr/m0n0wall/build81/tmp/mfsrootpc.gz /usr/m0n0wall/build81/tmp/firmwaretmp/mfsroot.gz
	cp /usr/m0n0wall/build81/tmp/bootdir/{loader,loader.rc} /usr/m0n0wall/build81/tmp/firmwaretmp
	cp /usr/m0n0wall/build81/tmp/acpi.ko /usr/m0n0wall/build81/tmp/firmwaretmp
	cp /usr/m0n0wall/build81/m0n0fs/conf.default/config.xml /usr/m0n0wall/build81/tmp/firmwaretmp

	cd /usr/m0n0wall/build81/tmp
	dd if=/dev/zero of=image.bin bs=1k count=`du -d0 /usr/m0n0wall/build81/tmp/firmwaretmp  | cut -b1-5 | tr " " "+" | xargs -I {} echo '(2048)+{}' | bc`
	rm -rf /usr/m0n0wall/build81/tmp/firmwaretmp
	
	mdconfig -a -t vnode -f /usr/m0n0wall/build81/tmp/image.bin -u 30
	disklabel  -wn  /dev/md30 auto |  awk '/unused/{if (M==""){sub("unused","4.2BSD");M=1}}{print}' > md.label
        bsdlabel -m  i386 -R -B -b /usr/m0n0wall/build81/tmp/bootdir/boot /dev/md30 md.label
        newfs -b 8192 -f 1024 -O2 -U -o space -m 0 /dev/md30a
	mount /dev/md30a /mnt
	
	cp /usr/m0n0wall/build81/tmp/kernel.gz /mnt/
	cp /usr/m0n0wall/build81/tmp/mfsrootpc.gz /mnt/mfsroot.gz
	mkdir -p /mnt/boot/kernel
	cp /usr/m0n0wall/build81/tmp/bootdir/{loader,loader.rc} /mnt/boot
	cp /usr/m0n0wall/build81/tmp/acpi.ko /mnt/boot/kernel
	mkdir /mnt/conf
	cp /usr/m0n0wall/build81/m0n0fs/conf.default/config.xml /mnt/conf
	cd /usr/m0n0wall/build81/tmp
	umount /mnt
	mdconfig -d -u 30
	gzip -9f image.bin
	mkdir -p /usr/m0n0wall/build81/tmp/cdroot/boot/
	cp image.bin.gz /usr/m0n0wall/build81/tmp/cdroot/firmware.img
	mv image.bin.gz /usr/m0n0wall/build81/images/generic-pc-1.8.0.img
	 
	 
# Make ISO
	cd /usr/m0n0wall/build81/tmp
	mkdir -p /usr/m0n0wall/build81/tmp/cdroot/boot/kernel
	cp /usr/m0n0wall/build81/tmp/acpi.ko /usr/m0n0wall/build81/tmp/cdroot/boot/kernel
	cp /usr/m0n0wall/build81/tmp/bootdir/{cdboot,loader,loader.rc} /usr/m0n0wall/build81/tmp/cdroot/boot
	cp kernel.gz mfsroot.gz /usr/m0n0wall/build81/tmp/cdroot/ 
	mkisofs -b "boot/cdboot" -no-emul-boot -A "m0n0wall 1.8.0 CD-ROM image" \
        -c "boot/boot.catalog" -d -r -publisher "m0n0.ch" \
        -p "m0n0.ch" -V "m0n0wall_cd" -o "m0n0wall.iso" \
        /usr/m0n0wall/build81/tmp/cdroot
	mv m0n0wall.iso /usr/m0n0wall/build81/images/generic-pc-1.8.0.iso
