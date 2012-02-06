/*
	$Id$
	part of m0n0wall (http://m0n0.ch/wall)

	Copyright (C) 2003-2012 Manuel Kasper <mk@neon1.net> and Lennart Grahl <masterkeule@gmail.com>.
	All rights reserved.

	Redistribution and use in source and binary forms, with or without
	modification, are permitted provided that the following conditions are met:

	1. Redistributions of source code must retain the above copyright notice,
	   this list of conditions and the following disclaimer.

	2. Redistributions in binary form must reproduce the above copyright
	   notice, this list of conditions and the following disclaimer in the
	   documentation and/or other materials provided with the distribution.

	THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
	INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
	AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
	AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
	OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
	SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
	INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
	CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
	ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
	POSSIBILITY OF SUCH DAMAGE.
*/

#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <unistd.h>

/* usage: croen 1:job 2:interval 3:pidfile 4:cmd 5:silent */

int main(int argc, char *argv[]) {

	/* not enough arguments? exit! */
	if (argc < 6)
		exit(1);

	/* define vars */
	int interval, silent;
	FILE *pidfd;
	interval = atoi(argv[2]);
	silent = atoi(argv[5]);

	/* interval 0? exit! */
	if (interval == 0)
		exit(1);

	/* unset loads of CGI environment variables */
	unsetenv("CONTENT_TYPE"); unsetenv("GATEWAY_INTERFACE");
	unsetenv("REMOTE_USER"); unsetenv("REMOTE_ADDR");
	unsetenv("AUTH_TYPE"); unsetenv("SCRIPT_FILENAME");
	unsetenv("CONTENT_LENGTH"); unsetenv("HTTP_USER_AGENT");
	unsetenv("HTTP_HOST"); unsetenv("SERVER_SOFTWARE");
	unsetenv("HTTP_REFERER"); unsetenv("SERVER_PROTOCOL");
	unsetenv("REQUEST_METHOD"); unsetenv("SERVER_PORT");
	unsetenv("SCRIPT_NAME"); unsetenv("SERVER_NAME");

	/* go into background */
	if (daemon(0, 0) == -1)
		exit(1);

	/* write PID to file */
	pidfd = fopen(argv[3], "w");
	if (pidfd) {
		fprintf(pidfd, "%d\n", getpid());
		fclose(pidfd);
	}

	/* wait... */
	sleep(interval);

	/* write syslog message if required */
	if (!silent) {
		openlog("croen daemon", 0, LOG_NOTICE);
		syslog(LOG_NOTICE, "Job %s executed", argv[1]);
		closelog();
	}

	/* remove PID file */
	remove(argv[3]);

	/* execute */
	char command[255];
	snprintf(command, 255, "/usr/local/bin/php %s", argv[4]);
	system(command);
	
    return 0;
}
