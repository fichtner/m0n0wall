#!/usr/local/bin/php
<?php 
/*
	index.php
	part of m0n0wall (http://m0n0.ch/wall)
	
	Copyright (C) 2003-2004 Manuel Kasper <mk@neon1.net>.
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

require("globals.inc");
require("util.inc");
require("config.inc");
require("radius_authentication.inc") ;
require("radius_accounting.inc") ;

header("Expires: 0");
header("Cache-Control: no-store, no-cache, must-revalidate");
header("Cache-Control: post-check=0, pre-check=0", false);
header("Pragma: no-cache");

$orig_host = $_ENV['HTTP_HOST'];
$orig_request = $_ENV['CAPTIVE_REQPATH'];
$lockfile = "{$g['varrun_path']}/captiveportal.lock";
$clientip = $_ENV['REMOTE_ADDR'];

/* find MAC address for client */
if ($clientip) {
	$clientmac = arp_get_mac_by_ip($clientip);
	if (!$clientmac) {
		/* unable to find MAC address - shouldn't happen! - bail out */
		exit;
	}
}

if (portal_mac_fixed($clientmac)) {
	/* punch hole in ipfw for pass thru mac addresses */
	portal_allow($clientip, $clientmac,"unauthenticated") ;

} else if ($_POST['accept'] && file_exists("{$g['vardb_path']}/captiveportal_radius.db")) {

	/* authenticate against radius server */

	$fd = @fopen("{$g['vardb_path']}/captiveportal_radius.db","r");
	if($fd) {
		$line = trim(fgets($fd));
		if($line)
			list($radiusip,$radiusport,$radiuskey) = explode(",",$line) ;
	}
	fclose($fd) ;
	
	if($_POST['auth_user'] && $_POST['auth_pass']) {	
		$auth_val = RADIUS_AUTHENTICATION($_POST['auth_user'],
										  $_POST['auth_pass'],
							  			  $radiusip,$radiusport,
							  			  $radiuskey) ;
		if ($auth_val == 2) {
			portal_allow($clientip, $clientmac,$_POST['auth_user']) ;
			if(isset($config['captiveportal']['radacct_enable'])) {
				$auth_val = RADIUS_ACCOUNTING_START($_POST['auth_user'],
											  $radiusip,$radiusport,
											  $radiuskey) ;
			}							  
		} else {
			readfile("{$g['varetc_path']}/captiveportal-error.html");
		}
	} else {
		readfile("{$g['varetc_path']}/captiveportal-error.html");
	}

} else if ($_POST['accept'] && $clientip) {
	portal_allow($clientip, $clientmac,"unauthenticated") ;
} else if ($_POST['logout_id'] && ($clientmac == $_POST['logout_id']) ) {
	disconnect_client($_POST['logout_id']) ;
	echo <<<EOD
<HTML>
<HEAD><TITLE>Disconnecting...</TITLE></HEAD>
<BODY BGCOLOR="#435370">
<SPAN STYLE="color: #ffffff; font-family: Tahoma, Verdana, Arial, Helvetica, sans-serif; font-size: 11px;">
<B>You've been disconnected.</B>
</SPAN>
<SCRIPT LANGUAGE="JavaScript">
<!--
setTimeout('window.close();',5000) ;
-->
</SCRIPT>
</BODY>
</HTML>

EOD;
} else if (($_ENV['SERVER_PORT'] != 8001) && isset($config['captiveportal']['httpslogin'])) {
	/* redirect to HTTPS login page */
	header("Location: https://{$config['captiveportal']['httpsname']}:8001/?redirurl=" . urlencode("http://{$orig_host}{$orig_request}"));
} else {
	/* display captive portal page */
	$htmltext = file_get_contents("{$g['varetc_path']}/captiveportal.html");
	
	/* substitute variables */
	if (isset($config['captiveportal']['httpslogin']))
		$htmltext = str_replace("\$PORTAL_ACTION\$", "https://{$config['captiveportal']['httpsname']}:8001/", $htmltext);
	else
		$htmltext = str_replace("\$PORTAL_ACTION\$", "", $htmltext);
	
	if (preg_match("/redirurl=(.*)/", $orig_request, $matches))
		$redirurl = urldecode($matches[1]);
	else
		$redirurl = "http://{$orig_host}{$orig_request}";
	$htmltext = str_replace("\$PORTAL_REDIRURL\$", htmlspecialchars($redirurl), $htmltext);
	
	echo $htmltext;
}

exit;

function portal_mac_fixed($clientmac) {
	global $g ;
	
	/* open captive portal mac db */
	if (file_exists("{$g['vardb_path']}/captiveportal_mac.db")) {
		$fd = @fopen("{$g['vardb_path']}/captiveportal_mac.db","r") ;
		if (!$fd) {
			return FALSE;
		}
		while (!feof($fd)) {
			$mac = trim(fgets($fd)) ;
			if(strcasecmp($clientmac, $mac) == 0) {
				fclose($fd) ;
				return TRUE ;
			}
		}
		fclose($fd) ;
	}
	return FALSE ;
}	

function portal_allow($clientip,$clientmac,$clientuser) {

	global $orig_host, $orig_request, $g, $config;

	/* user has accepted AUP - let him in */
	portal_lock();
	
	/* get next ipfw rule number */
	if (file_exists("{$g['vardb_path']}/captiveportal.nextrule"))
		$ruleno = trim(file_get_contents("{$g['vardb_path']}/captiveportal.nextrule"));
	if (!$ruleno)
		$ruleno = 10000;	/* first rule number */

	$saved_ruleno = $ruleno ;	
	
	/* add ipfw rules for layer 3 */
	exec("/sbin/ipfw add $ruleno set 2 skipto 50000 ip from $clientip to any in");
	exec("/sbin/ipfw add $ruleno set 2 skipto 50000 ip from any to $clientip out");
	
	/* add ipfw rules for layer 2 */
	$l2ruleno = $ruleno + 10000;
	exec("/sbin/ipfw add $l2ruleno set 3 deny all from $clientip to any not MAC any $clientmac layer2 in");
	exec("/sbin/ipfw add $l2ruleno set 3 deny all from any to $clientip not MAC $clientmac any layer2 out");
	
	/* read in passthru mac database */

	$cpdb = array() ;

	$fd = @fopen("{$g['vardb_path']}/captiveportal.db", "r");
	if ($fd) {
		while (!feof($fd)) {
			$line = trim(fgets($fd)) ;
			if($line) {
				$cpdb[] = explode(",",$line);
			}	
		}
		fclose($fd) ;
	}

	/* find entry and delete it */

	for ($i = 0; $i < count($cpdb); $i++) {
		if(!strcasecmp($cpdb[$i][3],$clientmac)) {
			if(isset($config['captiveportal']['radacct_enable']) &&
			   file_exists("{$g['vardb_path']}/captiveportal_radius.db")) {
				RADIUS_ACCOUNTING_STOP($cpdb[$i][1], // ruleno
									   $cpdb[$i][4], // username
									   $cpdb[$i][0], // start time
									   $config['captiveportal']['radiusip'],
									   $config['captiveportal']['radiusport'],
									   $config['captiveportal']['radiuskey'] ) ;
			}					   
			mwexec("/sbin/ipfw delete " . $cpdb[$i][1] . " " . ($cpdb[$i][1]+10000));
			unset($cpdb[$i]) ;
			break;
		}
	}	

	/* rewrite information to database */
	$fd = @fopen("{$g['vardb_path']}/captiveportal.db", "w");
	if ($fd) {
		foreach ($cpdb as $cpent) {
			fwrite($fd, join(",", $cpent) . "\n");
		}
		/* write in this new entry for clientmac */
		fwrite($fd, time().",{$ruleno},{$clientip},{$clientmac},{$clientuser}\n") ;
		fclose($fd);
	}
	
	/* write next rule number */
	$fd = @fopen("{$g['vardb_path']}/captiveportal.nextrule", "w");
	if ($fd) {
		$ruleno++;
		if ($ruleno > 19899)
			$ruleno = 10000;	/* wrap around */
		fwrite($fd, $ruleno);
		fclose($fd);
	}
	
	portal_unlock();
	
	/* redirect user to desired destination */
	if ($config['captiveportal']['redirurl'])
		$redirurl = $config['captiveportal']['redirurl'];
	else if ($_POST['redirurl'])
		$redirurl = $_POST['redirurl'];
	else
		$redirurl = "http://{$orig_host}{$orig_request}";
	
	if(isset($config['captiveportal']['logoutwin_enable'])) {
		
		if (isset($config['captiveportal']['httpslogin']))
			$logouturl = "https://{$config['captiveportal']['httpsname']}:8001/";
		else
			$logouturl = "http://{$config['interfaces'][$config['captiveportal']['interface']]['ipaddr']}:8000/";
		
		echo <<<EOD
<HTML>
<HEAD><TITLE>Redirecting...</TITLE></HEAD>
<BODY>
<SPAN STYLE="font-family: Tahoma, Verdana, Arial, Helvetica, sans-serif; font-size: 11px;">
<B>Redirecting to <A HREF="{$redirurl}">{$redirurl}</A>...</B>
</SPAN>
<SCRIPT LANGUAGE="JavaScript">
<!--
LogoutWin = window.open('', 'Logout', 'toolbar=0,scrollbars=0,location=0,statusbar=0,menubar=0,resizable=0,width=256,height=64');
if (LogoutWin) {
	LogoutWin.document.write('<HTML>');
	LogoutWin.document.write('<HEAD><TITLE>Logout</TITLE></HEAD>') ;
	LogoutWin.document.write('<BODY BGCOLOR="#435370">');
	LogoutWin.document.write('<DIV ALIGN="center" STYLE="color: #ffffff; font-family: Tahoma, Verdana, Arial, Helvetica, sans-serif; font-size: 11px;">') ;
	LogoutWin.document.write('<B>Click the button below to disconnect</B><P>');
	LogoutWin.document.write('<FORM METHOD="POST" ACTION="{$logouturl}">');
	LogoutWin.document.write('<INPUT NAME="logout_id" TYPE="hidden" VALUE="{$clientmac}">');
	LogoutWin.document.write('<INPUT NAME="logout" TYPE="submit" VALUE="Logout">');
	LogoutWin.document.write('</FORM>');
	LogoutWin.document.write('</DIV></BODY>');
	LogoutWin.document.write('</HTML>');
	LogoutWin.document.close();
}

document.location.href="{$redirurl}";
-->
</SCRIPT>
</BODY>
</HTML>

EOD;
	} else {
		header("Location: " . $redirurl); 
	}
}

/* lock captive portal information, decide that the lock file is stale after
   10 seconds */
function portal_lock() {
	
	global $lockfile;
	
	$n = 0;
	while ($n < 10) {
		/* open the lock file in append mode to avoid race condition */
		if ($fd = @fopen($lockfile, "x")) {
			/* succeeded */
			fclose($fd);
			return;
		} else {
			/* file locked, wait and try again */
			sleep(1);
			$n++;
		}
	}
}

/* unlock captive portal information file */
function portal_unlock() {
	
	global $lockfile;
	
	if (file_exists($lockfile))
		unlink($lockfile);
}

/* remove a single client by mac address
   by Dinesh Nair Thu Jul 29 18:46:38 MYT 2004
 */
function disconnect_client($macaddr) {
	
	global $g, $config;
	
	portal_lock();
	
	/* read database */
	$cpdb = array() ;
	$fd = @fopen("{$g['vardb_path']}/captiveportal.db", "r");
	if ($fd) {
		while (!feof($fd)) {
			$line = trim(fgets($fd)) ;
			if($line) {
				$cpdb[] = explode(",",$line);
			}	
		}
		fclose($fd) ;
	}
	
	/* find entry */	
	for ($i = 0; $i < count($cpdb); $i++) {
		if ($cpdb[$i][3] == $macaddr) {
			/* this client needs to be deleted - remove ipfw rules */
			if(isset($config['captiveportal']['radacct_enable']) &&
			   file_exists("{$g['vardb_path']}/captiveportal_radius.db")) {
				RADIUS_ACCOUNTING_STOP($cpdb[$i][1], // ruleno
									   $cpdb[$i][4], // username
									   $cpdb[$i][0], // start time
									   $config['captiveportal']['radiusip'],
									   $config['captiveportal']['radiusport'],
									   $config['captiveportal']['radiuskey'] ) ;
			}					   
			mwexec("/sbin/ipfw delete " . $cpdb[$i][1] . " " . ($cpdb[$i][1]+10000));
			unset($cpdb[$i]);
			break;
		}
	}
	
	/* rewrite information to database */
	$fd = @fopen("{$g['vardb_path']}/captiveportal.db", "w");
	if ($fd) {
		foreach ($cpdb as $cpent) {
			fwrite($fd, join(",", $cpent) . "\n");
		}
	}
	
	portal_unlock();
}
?>
