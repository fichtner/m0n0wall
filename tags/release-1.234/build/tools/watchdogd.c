/*
 * Copyright (c) 2002-2003, Dirk-Willem van Gulik, All Rights Reserved. See
 * http://www.webweaving.org/LICENSE for the licence.
 *
 * Adopted for NS Geode CPU watchdog by Marcel Wiget
 */
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <stdlib.h>
#include <signal.h>
#include <syslog.h>
#include <err.h>

#include <sys/watchdog.h>

static int      fd = 0;

#ifndef PIDFILE
#define PIDFILE "/var/run/watchdogd.pid"
#endif

static void
shutdown(int p)
{
	/* Try to switch off */
	unsigned        u = 0;
	if ((fd) && (ioctl(fd, WDIOCPATPAT, &u) < 0))
		syslog(LOG_EMERG, "Failed to switch off the watchdog: %s",
		       strerror(errno));
	syslog(LOG_CRIT, "Watchdog disabled");
	exit(0);
}

int
main(int argc, char **argv)
{
	if (argc != 1) {
		fprintf(stderr, "Usage: %s\n", argv[0]);
		exit(1);
	};

	fd = open("/dev/" _PATH_WATCHDOG, O_RDWR);
	if (fd < 0)
		err(1, "/dev/elan-mmcr");

	if (signal(SIGTERM, &shutdown) < 0)
		err(1, "signal");

	if (daemon(0, 0) < 0)
		err(1, "daemon");

	openlog(argv[0], LOG_CONS, LOG_DAEMON);

	if (1) {
		FILE           *pf;
		if (
		    ((pf = fopen(PIDFILE, "w")) == NULL) ||
		    (fprintf(pf, "%d\n", getpid()) <= 0) ||
		    (fclose(pf) != 0)
			)
			syslog(LOG_ERR, "Failed to write pid file " PIDFILE ": %s",
			       strerror(errno));
	}

	syslog(LOG_CRIT, "Watchdog enabled");
	while (1) {
		unsigned        u = WD_ACTIVE | WD_TO_32SEC;
		if (ioctl(fd, WDIOCPATPAT, &u))
			syslog(LOG_EMERG, "Failed to reset the watchdog");
		sleep(16);
	}

	/* NOT REACHED */
	exit(0);
}
