#!/usr/local/bin/bash

# get build env ready
	pkg_add -r subversion cdrtools curl autoconf213 autoconf262
	rehash
	cd  /usr/m0n0wall/build81
	svn export http://svn.m0n0.ch/wall/branches/freebsd8/
 
# make filesystem structure for image
	mkdir  m0n0fs tmp images
	cd m0n0fs
	mkdir -p etc/rc.d/ bin cf conf.default dev etc ftmp mnt proc root sbin tmp var libexec lib usr/bin usr/lib usr/libexec usr/local usr/sbin usr/share usr/local/bin usr/local/captiveportal usr/local/lib usr/local/sbin/.libs usr/local/www usr/share/misc boot/kernel
 
# insert svn files to filesystem
	cp -r ../freebsd8/phpconf/rc.* etc/
	cp -r ../freebsd8/phpconf/inc etc/
	cp -r ../freebsd8/etc/* etc
	cp -r ../freebsd8/webgui/ usr/local/www/
	cp -r ../freebsd8/captiveportal usr/local/
 
# set permissions
	chmod -R 0755 usr/local/www/* usr/local/captiveportal/*
	chmod a+rx etc/rc.reboot
 
# create links
	ln -s /cf/conf conf
	ln -s /var/run/htpasswd usr/local/www/.htpasswd
 
# configure build information
	echo "generic-pc-cdrom" > etc/platform
	date > etc/version.buildtime
	echo "1.8.0b0" > etc/version
 
# get and set current default configuration
	curl http://m0n0.ch/wall/downloads/config.xml > conf.default/config.xml
	cp conf.default/config.xml conf/config.xml
 
# insert termcap and zoneinfo files
	cp /usr/share/misc/termcap usr/share/misc
 
# do zoneinfo.tgz and dev fs
	cd tmp 
	cp /usr/m0n0wall/build81/freebsd8/build/newbuild/files/zoneinfo.tgz /usr/m0n0wall/build81/m0n0fs/usr/share
	perl /usr/m0n0wall/build81/freebsd8/build/minibsd/mkmini.pl /usr/m0n0wall/build81/freebsd8/build/minibsd/m0n0wall.files  / /usr/m0n0wall/build81/m0n0fs/
	tar -xzf /usr/m0n0wall/build81/freebsd8/build/newbuild/files/dev.tgz -C /usr/m0n0wall/build81/m0n0fs/

# create php.ini	
	cp /usr/m0n0wall/build81/freebsd8/build/newbuild/files/php.ini /usr/m0n0wall/build81/m0n0fs/usr/local/lib/php.ini

# create login.conf
	cp /usr/m0n0wall/build81/freebsd8/build/newbuild/files/login.conf /usr/m0n0wall/build81/m0n0fs/etc/
	
# create missing etc files
	tar -xzf /usr/m0n0wall/build81/freebsd8/build/newbuild/files/etcadditional.tgz -C /usr/m0n0wall/build81/m0n0fs/
	cp /usr/m0n0wall/build81/freebsd8/build/newbuild/files/rc /usr/m0n0wall/build81/m0n0fs/etc
	
	cd ..
	
