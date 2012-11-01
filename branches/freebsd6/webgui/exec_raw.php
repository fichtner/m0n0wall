#!/usr/local/bin/php
<?php
/*
	$Id$
	part of m0n0wall (http://m0n0.ch/wall)

	Copyright (C) 2003-2012 Manuel Kasper <mk@neon1.net>.
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
	
	
	Note: for CSRF protection, this script cannot be called directly with a
	GET parameter anymore. You must first call the script (GET) without any parameters
	to obtain a current token, and then call it again (POST) while passing the token
	value as the parameter __csrf_magic.
	
	Minimal example in Perl:
	
	#!/usr/bin/perl

	use LWP::UserAgent;

	my $m0n0wall_ip = "192.168.1.1";
	my $m0n0wall_user = "admin";
	my $m0n0wall_pass = "mono";
	my $cmd = "dmesg";

	my $ua = LWP::UserAgent->new;
	$ua->credentials("$m0n0wall_ip:80", ".", $m0n0wall_user, $m0n0wall_pass);

	# get new CSRF magic token
	my $res = $ua->get("http://$m0n0wall_ip/exec_raw.php");
	my $csrftoken = $res->content;

	# make a request to exec_raw.php
	$res = $ua->post("http://$m0n0wall_ip/exec_raw.php", {
		'__csrf_magic' => $csrftoken,
		'cmd' => $cmd
	});
	print $res->content;
	
*/
require("guiconfig.inc");

header("Content-Type: text/plain");

if ($_POST) {
	putenv("PATH=/bin:/sbin:/usr/bin:/usr/sbin:/usr/local/bin:/usr/local/sbin");
	passthru($_POST['cmd']);
} else {
	echo csrf_get_tokens();
}

exit(0);
?>