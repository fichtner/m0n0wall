#!/usr/local/bin/php
<?php 
/*
	$Id$
	part of m0n0wall (http://m0n0.ch/wall)
	
	Copyright (C) 2003-2006 Manuel Kasper <mk@neon1.net>.
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

require_once("functions.inc");
require_once("radius_authentication.inc");
require_once("radius_accounting.inc");

header("Expires: 0");
header("Cache-Control: no-store, no-cache, must-revalidate");
header("Cache-Control: post-check=0, pre-check=0", false);
header("Pragma: no-cache");

$orig_host = $_ENV['HTTP_HOST'];
$orig_request = $_ENV['CAPTIVE_REQPATH'];
$clientip = $_ENV['REMOTE_ADDR'];

if (!$clientip) {
	/* not good - bail out */
	exit;
}

if (isset($config['captiveportal']['httpslogin']))
	$ourhostname = $config['captiveportal']['httpsname'] . ":8001";
else
	$ourhostname = $config['interfaces'][$config['captiveportal']['interface']]['ipaddr'] . ":8000";

if ($orig_host != $ourhostname) {
	/* the client thinks it's connected to the desired web server, but instead
	   it's connected to us. Issue a redirect... */
	  
	if (isset($config['captiveportal']['httpslogin']))
		header("Location: https://{$ourhostname}/?redirurl=" . urlencode("http://{$orig_host}{$orig_request}"));
	else
		header("Location: http://{$ourhostname}/?redirurl=" . urlencode("http://{$orig_host}{$orig_request}"));
	
	exit;
}

if (preg_match("/redirurl=(.*)/", $orig_request, $matches))
	$redirurl = urldecode($matches[1]);
if ($_POST['redirurl'])
	$redirurl = $_POST['redirurl'];

$macfilter = !isset($config['captiveportal']['nomacfilter']);

if (file_exists("{$g['vardb_path']}/captiveportal_radius.db")) {
	$radius_enable = TRUE;
	if ($radius_enable && $macfilter && isset($config['captiveportal']['radmac_enable']))
		$radmac_enable = TRUE;
}

/* find MAC address for client */
$clientmac = arp_get_mac_by_ip($clientip);
if (!$clientmac && $macfilter) {
	/* unable to find MAC address - shouldn't happen! - bail out */
	captiveportal_logportalauth("unauthenticated","noclientmac",$clientip,"ERROR");
	exit;
}

if ($_POST['logout_id']) {
	disconnect_client($_POST['logout_id']);
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
} else if ($clientmac && $macfilter && portal_mac_fixed($clientmac)) {
	/* punch hole in ipfw for pass thru mac addresses */
	portal_allow($clientip, $clientmac, "unauthenticated");
	exit;

} else if ($clientmac && $radmac_enable && portal_mac_radius($clientmac,$clientip)) {
	/* radius functions handle everything so we exit here since we're done */
	exit;

} else if ($_POST['accept'] && $radius_enable) {

	if ($_POST['auth_user'] && $_POST['auth_pass']) {
		$auth_list = radius($_POST['auth_user'],$_POST['auth_pass'],$clientip,$clientmac,"USER LOGIN");

		if ($auth_list['auth_val'] == 1) {
			captiveportal_logportalauth($_POST['auth_user'],$clientmac,$clientip,"ERROR",$auth_list['error']);
			portal_reply_page($redirurl, "error", $auth_list['error']);
		}
		else if ($auth_list['auth_val'] == 3) {
			captiveportal_logportalauth($_POST['auth_user'],$clientmac,$clientip,"FAILURE",$auth_list['reply_message']);
			portal_reply_page($redirurl, "error", $auth_list['reply_message']);
		}
	} else {
		captiveportal_logportalauth($_POST['auth_user'],$clientmac,$clientip,"ERROR");
		portal_reply_page($redirurl, "error");
	}
	
} else if ($_POST['accept'] && $config['captiveportal']['auth_method'] == "local") {

	//check against local usermanager
	$userdb = &$config['captiveportal']['user'];

	$loginok = false;

	//erase expired accounts
	if (is_array($userdb)) {
		$moddb = false;
		for ($i = 0; $i < count($userdb); $i++) {
			if ($userdb[$i]['expirationdate'] && (strtotime("-1 day") > strtotime($userdb[$i]['expirationdate']))) {
				unset($userdb[$i]);
				$moddb = true;
			}
		}
		if ($moddb)
			write_config();
			
		$userdb = &$config['captiveportal']['user'];
		
		for ($i = 0; $i < count($userdb); $i++) {
			if (($userdb[$i]['name'] == $_POST['auth_user']) && ($userdb[$i]['password'] == md5($_POST['auth_pass']))) {
				$loginok = true;
				break;
			}
		}
	}

	if ($loginok){
		captiveportal_logportalauth($_POST['auth_user'],$clientmac,$clientip,"LOGIN");
		portal_allow($clientip, $clientmac,$_POST['auth_user']);
	} else {
		captiveportal_logportalauth($_POST['auth_user'],$clientmac,$clientip,"FAILURE");
		portal_reply_page($redirurl, "error");
	}
} else if ($_POST['accept'] && $clientip) {
	captiveportal_logportalauth("unauthenticated",$clientmac,$clientip,"ACCEPT");
	portal_allow($clientip, $clientmac, "unauthenticated");
} else {
	/* display captive portal page */
	portal_reply_page($redirurl, "login");
}

exit;

function portal_reply_page($redirurl, $type = null, $message = null) {
	global $g, $config;

	/* Get captive portal layout */
	if ($type == "login") 
		$htmltext = file_get_contents("{$g['varetc_path']}/captiveportal.html");
	else 
		$htmltext = file_get_contents("{$g['varetc_path']}/captiveportal-error.html");

	/* substitute other variables */
	if (isset($config['captiveportal']['httpslogin']))
		$htmltext = str_replace("\$PORTAL_ACTION\$", "https://{$config['captiveportal']['httpsname']}:8001/", $htmltext);
	else
		$htmltext = str_replace("\$PORTAL_ACTION\$", "http://{$config['interfaces'][$config['captiveportal']['interface']]['ipaddr']}:8000/", $htmltext);

	$htmltext = str_replace("\$PORTAL_REDIRURL\$", htmlspecialchars($redirurl), $htmltext);
	$htmltext = str_replace("\$PORTAL_MESSAGE\$", htmlspecialchars($message), $htmltext);

	echo $htmltext;
}

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

function portal_mac_radius($clientmac,$clientip) {
	global $config ;

	$radmac_secret = $config['captiveportal']['radmac_secret'];

	/* authentication against the radius server */
	$auth_list = radius($clientmac,$radmac_secret,$clientip,$clientmac,"MACHINE LOGIN");
	if ($auth_list['auth_val'] == 2) {
		return TRUE;
	}
	return FALSE;
}

function portal_allow($clientip,$clientmac,$clientuser,$password = null, $session_timeout = null, $idle_timeout = null, $url_redirection = null, $session_terminate_time = null)  {

	global $redirurl, $g, $config;

	if ((isset($config['captiveportal']['noconcurrentlogins'])) && ($clientuser != 'unauthenticated'))
		kick_concurrent_logins($clientuser);

	captiveportal_lock();
	
	$ruleno = get_next_ipfw_ruleno();

	/* generate unique session ID */
	$tod = gettimeofday();
	$sessionid = substr(md5(mt_rand() . $tod['sec'] . $tod['usec'] . $clientip . $clientmac), 0, 16);
	
	/* add ipfw rules for layer 3 */
	exec("/sbin/ipfw add $ruleno set 2 skipto 50000 ip from $clientip to any in");
	exec("/sbin/ipfw add $ruleno set 2 skipto 50000 ip from any to $clientip out");
	
	/* add ipfw rules for layer 2 */
	if (!isset($config['captiveportal']['nomacfilter'])) {
		$l2ruleno = $ruleno + 10000;
		exec("/sbin/ipfw add $l2ruleno set 3 deny all from $clientip to any not MAC any $clientmac layer2 in");
		exec("/sbin/ipfw add $l2ruleno set 3 deny all from any to $clientip not MAC $clientmac any layer2 out");
	}
	
	/* read in client database */
	$cpdb = captiveportal_read_db();
	
	$radiusservers = captiveportal_get_radius_servers();

	/* find an existing entry and delete it */
	for ($i = 0; $i < count($cpdb); $i++) {
		if(!strcasecmp($cpdb[$i][2],$clientip)) {
			if(isset($config['captiveportal']['radacct_enable']) && isset($radiusservers[0])) {
				RADIUS_ACCOUNTING_STOP($cpdb[$i][1], // ruleno
									   $cpdb[$i][4], // username
									   $cpdb[$i][5], // sessionid
									   $cpdb[$i][0], // start time
									   $radiusservers[0]['ipaddr'],
									   $radiusservers[0]['acctport'],
									   $radiusservers[0]['key'],
									   $cpdb[$i][2], // clientip
									   $cpdb[$i][3], // clientmac
									   13); // Port Preempted
			}
			mwexec("/sbin/ipfw delete " . $cpdb[$i][1] . " " . ($cpdb[$i][1]+10000));
			unset($cpdb[$i]);
			break;
		}
	}	

	/* encode password in Base64 just in case it contains commas */
	$bpassword = base64_encode($password);
	$cpdb[] = array(time(), $ruleno, $clientip, $clientmac, $clientuser, $sessionid, $bpassword, $session_timeout, $idle_timeout, $session_terminate_time);

	/* rewrite information to database */
	captiveportal_write_db($cpdb);

	/* write next rule number */
	$fd = @fopen("{$g['vardb_path']}/captiveportal.nextrule", "w");
	if ($fd) {
		$ruleno++;
		if ($ruleno > 19899)
			$ruleno = 10000;	/* wrap around */
		fwrite($fd, $ruleno);
		fclose($fd);
	}
	
	captiveportal_unlock();
	
	/* redirect user to desired destination */
	if ($url_redirection)
		$my_redirurl = $url_redirection;
	else if ($config['captiveportal']['redirurl'])
		$my_redirurl = $config['captiveportal']['redirurl'];
	else
		$my_redirurl = $redirurl;
	
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
<B>Redirecting to <A HREF="{$my_redirurl}">{$my_redirurl}</A>...</B>
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
	LogoutWin.document.write('<INPUT NAME="logout_id" TYPE="hidden" VALUE="{$sessionid}">');
	LogoutWin.document.write('<INPUT NAME="logout" TYPE="submit" VALUE="Logout">');
	LogoutWin.document.write('</FORM>');
	LogoutWin.document.write('</DIV></BODY>');
	LogoutWin.document.write('</HTML>');
	LogoutWin.document.close();
}

document.location.href="{$my_redirurl}";
-->
</SCRIPT>
</BODY>
</HTML>

EOD;
	} else {
		header("Location: " . $my_redirurl); 
	}
	
	return $sessionid;
}

/* Ensure that only one username is used by one client at a time
 * by Paul Taylor
 */
function kick_concurrent_logins($user) {

	captiveportal_lock();

	/* read database */
	$cpdb = captiveportal_read_db();

	captiveportal_unlock();

	if (isset($cpdb)) {
		/* find duplicate entry */
		for ($i = 0; $i < count($cpdb); $i++) {
			if ($cpdb[$i][4] == $user) {
				/* This user was already logged in */
				disconnect_client($cpdb[$i][5],"CONCURRENT LOGIN - TERMINATING OLD SESSION",13);
			}
		}
	}
}

/* remove a single client by session ID
   by Dinesh Nair
 */
function disconnect_client($sessionid, $logoutReason = "LOGOUT", $term_cause = 1) {
	
	global $g, $config;
	
	captiveportal_lock();
	
	/* read database */
	$cpdb = captiveportal_read_db();
	
	$radiusservers = captiveportal_get_radius_servers();
	
	/* find entry */	
	for ($i = 0; $i < count($cpdb); $i++) {
		if ($cpdb[$i][5] == $sessionid) {
			/* this client needs to be deleted - remove ipfw rules */
			if(isset($config['captiveportal']['radacct_enable']) && isset($radiusservers[0])) {
				RADIUS_ACCOUNTING_STOP($cpdb[$i][1], // ruleno
									   $cpdb[$i][4], // username
									   $cpdb[$i][5], // sessionid
									   $cpdb[$i][0], // start time
									   $radiusservers[0]['ipaddr'],
									   $radiusservers[0]['acctport'],
									   $radiusservers[0]['key'],
									   $cpdb[$i][2], // clientip
									   $cpdb[$i][3], // clientmac
									   $term_cause);
			}
			mwexec("/sbin/ipfw delete " . $cpdb[$i][1] . " " . ($cpdb[$i][1]+10000));
			captiveportal_logportalauth($cpdb[$i][4],$cpdb[$i][3],$cpdb[$i][2],$logoutReason);
			unset($cpdb[$i]);
			break;
		}
	}
	
	/* rewrite information to database */
	captiveportal_write_db($cpdb);
	
	captiveportal_unlock();
}

function get_next_ipfw_ruleno() {

	global $g;

	/* get next ipfw rule number */
	if (file_exists("{$g['vardb_path']}/captiveportal.nextrule"))
		$ruleno = trim(file_get_contents("{$g['vardb_path']}/captiveportal.nextrule"));
	if (!$ruleno)
		$ruleno = 10000;	/* first rule number */

	return $ruleno;
}

?>
