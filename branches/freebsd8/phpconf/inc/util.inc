<?php
/*
	$Id$
	part of m0n0wall (http://m0n0.ch/wall)
	
	Copyright (C) 2003-2007 Manuel Kasper <mk@neon1.net>.
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

require_once("IPv6.php");

/* kill a process by pid file */
function killbypid($pidfile) {
	sigkillbypid($pidfile, "TERM");
	unlink_if_exists($pidfile);
}

/* sigkill a process by pid file */
function sigkillbypid($pidfile, $sig) {
	if (file_exists($pidfile)) {
		mwexec("/bin/kill -s $sig `/bin/cat " . $pidfile . "`");
	}
}

/* kill a process by name */
function killbyname($procname) {
	return mwexec("/usr/bin/killall " . escapeshellarg($procname));
}

/* kill a process by name */
function sigkillbyname($procname, $sig) {
	return mwexec("/usr/bin/killall -{$sig} " . escapeshellarg($procname));
}

/* return the subnet address given a host address and a subnet bit count */
function gen_subnet($ipaddr, $bits) {
	if (!is_ipaddr($ipaddr) || !is_numeric($bits))
		return "";
	
	return long2ip(ip2long($ipaddr) & gen_subnet_mask_long($bits));
}

/* return the subnet address given a host address and a subnet bit count, IPv6 variant */
function gen_subnet6($ipaddr, $bits) {
	if (!is_ipaddr6($ipaddr) || !is_numeric($bits))
		return '';
	
	return Net_IPv6::getNetmask($ipaddr, $bits);
}
/* return the broadcast address given a host address and a subnet bit count, IPv6 variant */
function gen_subnet_max6($ipaddr, $bits) {
	if (!is_ipaddr6($ipaddr) || !is_numeric($bits))
		return '';
	$ipasbin = Net_IPv6::_ip2Bin(Net_IPv6::removeNetmaskSpec($ipaddr));
	$binBcast = substr($ipasbin,0,$bits) . str_repeat('1',128 - $bits);
	
	return  Net_IPv6::_Bin2ip($binBcast);
}
/* return the highest (broadcast) address in the subnet given a host address and a subnet bit count */
function gen_subnet_max($ipaddr, $bits) {
	if (!is_ipaddr($ipaddr) || !is_numeric($bits))
		return "";
	
	return long2ip(ip2long($ipaddr) | ~gen_subnet_mask_long($bits));
}

/* returns a subnet mask (long given a bit count) */
function gen_subnet_mask_long($bits) {
	$sm = 0;
	for ($i = 0; $i < $bits; $i++) {
		$sm >>= 1;
		$sm |= 0x80000000;
	}
	return $sm;
}

/* same as above but returns a string */
function gen_subnet_mask($bits) {
	return long2ip(gen_subnet_mask_long($bits));
}

function is_numericint($arg) {
	return (preg_match("/[^0-9]/", $arg) ? false : true);
}

/* returns true if $ipaddr is a valid IPv4 or IPv6 address */
function is_ipaddr4or6($ipaddr) {
	if (is_ipaddr($ipaddr)) 
		return true;
	else if (ipv6enabled()) 
		return is_ipaddr6($ipaddr);
	else 
		return false;	
}

/* returns true if $ipaddr is a valid dotted IPv4 address */
function is_ipaddr($ipaddr) {
	if (!is_string($ipaddr))
		return false;
		
	$ip_long = ip2long($ipaddr);
	$ip_reverse = long2ip($ip_long);
 
	if ($ipaddr == $ip_reverse)
		return true;
	else
		return false;
}

/* returns true if $ipaddr is a valid IPv6 address with not prefix */
function is_ipaddr6($ipaddr) {
	if (!Net_IPv6::getNetmaskSpec($ipaddr)) {
		return Net_IPv6::checkIPv6($ipaddr);
	} else {
	return false;
	}
}

function is_ipaddr6andprefix($ipaddr) {
	if (Net_IPv6::getNetmaskSpec($ipaddr)) {
		return Net_IPv6::checkIPv6($ipaddr);
	} else {
	return false;
	}
}

/* returns MTU of the interface */
function get_interface_mtu($interface) {
       $mtu = null;
       $p = @popen("/sbin/ifconfig " . escapeshellarg($interface), "r");
       if ($p === false)
               return null;
       while (!feof($p)) {
               $line = fgets($p);
               if (preg_match("/mtu (\d+)/", $line, $matches)) {
                       $mtu = $matches[1];
                       break;
               }
       }
       pclose($p);
       
       return $mtu;
}

function isInNetmask6($ip, $netmask, $bits=null) {
	return Net_IPv6::isInNetmask($ip, $netmask, $bits);
}

function ipv6Uncompress($ip) {
	return Net_IPv6::Uncompress($ip);
}

/* returns true if $ipaddr is a valid dotted IPv4 address or an alias thereof */
function is_ipaddroralias($ipaddr) {
	
	global $aliastable;
	
	if (isset($aliastable[$ipaddr]) && is_ipaddr($aliastable[$ipaddr]))
		return true;
	else
		return is_ipaddr($ipaddr);
}

/* returns true if $ipaddr is a valid dotted IPv4 address or any alias */
function is_ipaddroranyalias($ipaddr) {
	
	global $aliastable;
	
	if (isset($aliastable[$ipaddr]))
		return true;
	else
		return is_ipaddr($ipaddr);
}

/* returns true if $subnet is a valid subnet in CIDR format */
function is_subnet($subnet) {
	if (!is_string($subnet))
		return false;
		
	list($hp,$np) = explode('/', $subnet);
	
	if (!is_ipaddr($hp))
		return false;
	
	if (!is_numeric($np) || ($np < 1) || ($np > 32))
		return false;
		
	return true;
}

/* returns true if $subnet is a valid IPv6 subnet in CIDR format */
function is_subnet6($subnet) {
	if (!is_string($subnet))
		return false;
		
	list($hp,$np) = explode('/', $subnet);
	
	if (!is_ipaddr6($hp))
		return false;
	
	if (!is_numeric($np) || ($np < 1) || ($np > 128))
		return false;
		
	return true;
}

/* returns true if $subnet is a valid subnet in CIDR format or an alias thereof */
function is_subnetoralias($subnet) {
	
	global $aliastable;
	
	if (isset($aliastable[$subnet]) && is_subnet($aliastable[$subnet]))
		return true;
	else
		return is_subnet($subnet);
}

/* returns true if $hostname is a valid hostname */
function is_hostname($hostname) {
	if (!is_string($hostname))
		return false;
		
	if (preg_match("/^[a-z0-9\-]+$/i", $hostname))
		return true;
	else
		return false;
}

/* returns true if $domain is a valid domain name */
function is_domain($domain) {
	if (!is_string($domain))
		return false;
		
	if (preg_match("/^([a-z0-9\-]+\.?)*$/i", $domain))
		return true;
	else
		return false;
}

/* returns true if $uname is a valid DynDNS username */
function is_dyndns_username($uname) {
	if (!is_string($uname))
		return false;
		
	if (preg_match("/[^a-z0-9\-.@_]/i", $uname))
		return false;
	else
		return true;
}

/* returns true if $macaddr is a valid MAC address */
function is_macaddr($macaddr) {
	if (!is_string($macaddr))
		return false;
		
	$maca = explode(":", $macaddr);
	if (count($maca) != 6)
		return false;
	
	foreach ($maca as $macel) {
		if (($macel === "") || (strlen($macel) > 2))
			return false;
		if (preg_match("/[^0-9a-f]/i", $macel))
			return false;
	}
	
	return true;
}

/* returns true if $duid is a valid DUID address */
/* RFC3315 says that a 'servers MUST NOT restrict DUIDs to the types in this Document' which means the length could be anything, though a mac length is a good min*/
function is_duid($duid) {
	if (!is_string($duid))
		return false;
		
	$duida = explode(":", $duid);
	if (count($duida) < 7)
		return false;
	
	foreach ($duida as $duidl) {
		if (($duidl === "") || (strlen($duidl) > 2))
			return false;
		if (preg_match("/[^0-9a-f]/i", $duidl))
			return false;
	}
	
	return true;
}

/* returns true if $name is a valid name for an alias */
function is_validaliasname($name) {
	if (!preg_match("/[^a-zA-Z0-9-]/", $name))
		return true;
	else
		return false;
}

/* returns true if $port is a valid TCP/UDP port */
function is_port($port) {
	if (!is_numericint($port))
		return false;
		
	if (($port < 1) || ($port > 65535))
		return false;
	else
		return true;
}

/* returns a list of interfaces with MAC addresses
   (skips VLAN and other virtual interfaces) */
function get_interface_list($ethernet = true, $wireless = true) {
	
	global $g;
	
	/* build interface list with netstat */
	exec("/usr/bin/netstat -inW -f link", $linkinfo);
	array_shift($linkinfo);
	
	/* get dmesg output in order to parse interface driver names from it */
	$dmesg = system_get_dmesg_boot();
	
	$iflist = array();
	
	foreach ($linkinfo as $link) {
		$alink = preg_split("/\s+/", $link);
		$ifname = chop($alink[0]);
		
		if (substr($ifname, -1) == "*")
			$ifname = substr($ifname, 0, strlen($ifname) - 1);
		
		if (preg_match("/^(ppp|sl|gif|faith|lo|ng|vlan|wlan|tun|enc)\d/", $ifname))
			continue;
		
		if (preg_match($g['wireless_regex'], $ifname)) {
			/* this is a wireless interface - should we include it? */
			if (!$wireless)
				continue;
		} else {
			/* this is not a wireless interface (so we assume it's Ethernet) */
			if (!$ethernet)
				continue;
		}
			
		$iflist[$ifname] = array();
		
		$iflist[$ifname]['mac'] = chop($alink[3]);
		$iflist[$ifname]['up'] = false;
		
		/* find out if the link on this interface is up */
		unset($ifinfo);
		exec("/sbin/ifconfig {$ifname}", $ifinfo);
		
		foreach ($ifinfo as $ifil) {
			if (preg_match("/status: (.*)$/", $ifil, $matches)) {
				if ($matches[1] == "active")
					$iflist[$ifname]['up'] = true;
				break;
			}
		}
		
		/* try to figure out driver name */
		if (preg_match("/$ifname:\s+<(.+?)>/", $dmesg, $matches)) {
			/* truncate at 40 characters; some of these get really long
			   and mess up the CLI and webGUI display */
			$drvname = $matches[1];
			if (strlen($drvname) > 40)
				$drvname = substr($drvname, 0, 40) . "...";
			$iflist[$ifname]['drvname'] = $drvname;
		}
	}
	
	return $iflist;
}

/* convert friendly interface name (WAN, OPTx, etc.) to real interface (fxp0, ng0, etc.) */
function convert_friendly_interface_to_real_interface_name($interface) {
	global $config;
	if($config['interfaces'][$interface]['ipaddr'] == "pppoe" or $config['interfaces'][$interface]['ipaddr'] == "pptp")
		return "ng0";
	$lc_interface = strtolower($interface);
	if($lc_interface == "lan") return $config['interfaces']['lan']['if'];
	if($lc_interface == "wan") return $config['interfaces']['wan']['if'];
	$ifdescrs = array();
	for ($j = 1; isset($config['interfaces']['opt' . $j]); $j++)
		$ifdescrs['opt' . $j] = "opt" . $j;
	foreach ($ifdescrs as $ifdescr => $ifname) {
		if(strtolower($ifname) == $lc_interface)
	    return $config['interfaces'][$ifname]['if'];
		if(strtolower($config['interfaces'][$ifname]['descr']) == $lc_interface)
			return $config['interfaces'][$ifname]['if'];
   }
   return $interface;
}

/* wrapper for exec() */
function mwexec($command) {

	global $g;
	
	if ($g['debug']) {
		if (!$_SERVER['REMOTE_ADDR'])
			echo "mwexec(): $command\n";
		passthru($command, $retval);
	} else {
		exec("$command > /dev/null 2>&1", $oarr, $retval);
	}
	
	return $retval; 
}

/* wrapper for exec() in background; returns PID of background process */
function mwexec_bg($command) {

	global $g;
	
	if ($g['debug']) {
		if (!$_SERVER['REMOTE_ADDR'])
			echo "mwexec_bg(): $command\n";
	}
	
	$pid = shell_exec("nohup $command > /dev/null 2>&1 & echo \$!");
	return $pid;
}

/* unlink a file, if it exists */
function unlink_if_exists($fn) {
	if (file_exists($fn))
		unlink($fn);
}

/* make a global alias table (for faster lookups) */
function alias_make_table() {
	
	global $config, $g, $aliastable;
	
	$aliastable = array();
	
	if (is_array($config['aliases']['alias'])) {
		foreach ($config['aliases']['alias'] as $alias) {
			if ($alias['name'])
				$aliastable[$alias['name']] = $alias['address'];
		}
	}
}

/* check if an alias exists */
function is_alias($name) {
	
	global $aliastable;
	
	return isset($aliastable[$name]);
}

/* expand a host or network alias, if necessary */
function alias_expand($name) {
	
	global $aliastable;
	
	if (isset($aliastable[$name]))
		return $aliastable[$name];
	else if (is_ipaddr($name) || is_subnet($name) || is_ipaddr6($name) || is_subnet6($name))
		return $name;
	else
		return null;
}

/* expand a host alias, if necessary */
function alias_expand_host($name) {
	
	global $aliastable;
	
	if (isset($aliastable[$name]) && is_ipaddr($aliastable[$name]))
		return $aliastable[$name];
	else if (is_ipaddr($name))
		return $name;
	else
		return null;
}

/* expand a network alias, if necessary */
function alias_expand_net($name) {
	
	global $aliastable;
	
	if (isset($aliastable[$name]) && is_subnet($aliastable[$name]))
		return $aliastable[$name];
	else if (is_subnet($name))
		return $name;
	else
		return null;
}

/* find out whether two subnets overlap */
function check_subnets_overlap($subnet1, $bits1, $subnet2, $bits2) {

	if (!is_numeric($bits1))
		$bits1 = 32;
	if (!is_numeric($bits2))
		$bits2 = 32;

	if ($bits1 < $bits2)
		$relbits = $bits1;
	else
		$relbits = $bits2;
	
	$sn1 = gen_subnet_mask_long($relbits) & ip2long($subnet1);
	$sn2 = gen_subnet_mask_long($relbits) & ip2long($subnet2);
	
	if ($sn1 == $sn2)
		return true;
	else
		return false;
}

/* compare two IP addresses */
function ipcmp($a, $b) {
	if (ip2long($a) < ip2long($b))
		return -1;
	else if (ip2long($a) > ip2long($b))
		return 1;
	else
		return 0;
}

/* return true if $addr is in $subnet, false if not */
function ip_in_subnet($addr,$subnet) {
	list($ip, $mask) = explode('/', $subnet);
	$mask = 0xffffffff << (32 - $mask);
	return ((ip2long($addr) & $mask) == (ip2long($ip) & $mask));
}

/* verify (and remove) the digital signature on a file - returns 0 if OK */
function verify_digital_signature($fname) {

	global $g;

	return mwexec("/usr/local/bin/verifysig " .
		escapeshellarg("{$g['etc_path']}/pubkey.pem") . " " .
		escapeshellarg($fname));
}

/* obtain MAC address given an IP address by looking at the ARP table */
function arp_get_mac_by_ip($ip) {
	exec("/usr/sbin/arp -n {$ip}", $arpoutput);
	
	if ($arpoutput[0]) {
		$arpi = explode(" ", $arpoutput[0]);
		$macaddr = $arpi[3];
		if (is_macaddr($macaddr))
			return $macaddr;
		else
			return false;
	}
	
	return false;
}

function mac_format($clientmac) {
    $mac =explode(":", $clientmac);

    global $config;

    $mac_format = $config['captiveportal']['radmac_format'] ? $config['captiveportal']['radmac_format'] : false;

    switch($mac_format) {

        case 'singledash':
        return "$mac[0]$mac[1]$mac[2]-$mac[3]$mac[4]$mac[5]";

        case 'ietf':
        return "$mac[0]-$mac[1]-$mac[2]-$mac[3]-$mac[4]-$mac[5]";

        case 'cisco':
        return "$mac[0]$mac[1].$mac[2]$mac[3].$mac[4]$mac[5]";

        case 'unformatted':
        return "$mac[0]$mac[1]$mac[2]$mac[3]$mac[4]$mac[5]";

        default:
        return $clientmac;
    }
}

function resolve_retry($hostname, $retries = 5) {

	if (is_ipaddr($hostname))
		return $hostname;

	for ($i = 0; $i < $retries; $i++) {
		$ip = gethostbyname($hostname);
		
		if ($ip && $ip != $hostname) {
			/* success */
			return $ip;
		}
		
		sleep(1);
	}
	
	return false;
}

function subnet_size_to_cidr($size) {
	$cidr = 32;
	
	while ($size > 1) {
		$size /= 2;
		$cidr--;
	}
	
	return $cidr;
}

/* converts the given IPv4 address to a string of v6 hex nibbles ("xxxx:xxxx"),
   mostly for use in 6to4 calculations */
function convert_to_6to4_nibbles($ipaddr) {
	if (!is_ipaddr($ipaddr))
		return false;
	$octets = explode('.', $ipaddr);
	return sprintf('%02x%02x:%02x%02x',	$octets[0], $octets[1], $octets[2], $octets[3]);
}

/* generate a self-signed SSL cert/key (1024-bit, valid for 2 years) and return it in PEM format */
function generate_self_signed_cert($orgname, $cn) {
	global $g;
	
	/* check local clock first - must be set to something after the release date
	  (subtract a day to avoid TZ problems with early adopters) */
	$buildtime = trim(file_get_contents("{$g['etc_path']}/version.buildtime.unix")) - 86400;
	if (time() < $buildtime)
		return false;
	
	$dn = array(
	    "organizationName" => $orgname,
	    "commonName" => $cn
	);

	$config = array(
		"digest_alg" => "sha1",
		"private_key_bits" => 1024
	);

	$privkey = openssl_pkey_new($config);
	$csr = openssl_csr_new($dn, $privkey, $config);
	$sscert = openssl_csr_sign($csr, null, $privkey, 730, $config, time());

	openssl_x509_export($sscert, $certout);
	openssl_pkey_export($privkey, $pkeyout);

	/* sanity check */
	if (strpos($certout, "BEGIN CERTIFICATE") !== false && strpos($pkeyout, "BEGIN RSA PRIVATE KEY") !== false)
		return array('cert' => $certout, 'key' => $pkeyout);
	else
		return false;
}

/* check whether a DHCP client range is valid on an interface with a given IP address and subnet mask;
   return an array of error strings (or an empty array on success) */
function check_dhcp_range($if_ipaddr, $if_subnet, $range_from, $range_to) {
	$input_errors = array();
	$subnet_start = (ip2long($if_ipaddr) & gen_subnet_mask_long($if_subnet));
	$subnet_end = (ip2long($if_ipaddr) | (~gen_subnet_mask_long($if_subnet)));
	
	$rangefrom = ip2long($range_from);
	$rangeto = ip2long($range_to);
	$ifip = ip2long($if_ipaddr);
	
	if (($rangefrom < $subnet_start) || ($rangefrom > $subnet_end) ||
	    ($rangeto < $subnet_start) || ($rangeto > $subnet_end))
		$input_errors[] = "The specified range lies outside of the current subnet.";
	
	if ($rangefrom > $rangeto)
		$input_errors[] = "The range is invalid (start address higher than end address).";
	
	if (($rangefrom == $subnet_start) || ($rangeto == $subnet_end))
		$input_errors[] = "The network address (" . long2ip($subnet_start) . ") or the broadcast address (" . long2ip($subnet_end) . ") cannot be used in the range.";
	
	if ($ifip >= $rangefrom && $ifip <= $rangeto)
		$input_errors[] = "The interface's IP address cannot be included in the range.";
	
	return $input_errors;
}

function check_v6dhcp_range($if_ipaddr6, $if_subnet6, $v6range_from, $v6range_to) {
	$input_errors = array();
	
	if (is_ipaddr6($if_ipaddr6)) {
		/* check that range lies in current interface subnet */
		$if_v6subnet = gen_subnet6($if_ipaddr6, $if_subnet6);
		if (!isInNetmask6($v6range_from, $if_v6subnet, $if_subnet6) || !isInNetmask6($v6range_to, $if_v6subnet, $if_subnet6)) {
			$input_errors[] = "The specified IPv6 range lies outside of the current subnet (make sure the interface is configured with an IPv6 address before you enable the DHCP server).";
		}
	}
	
	/* check range start/end */
	$v6range_from_array = explode(":",ipv6Uncompress($v6range_from));
	$v6range_to_array = explode(":",ipv6Uncompress($v6range_to));
	for ($i=0; $i<=7; $i++)		{
	  if ($v6range_from_array[$i] > $v6range_to_array[$i]) {
			$input_errors[] = "The IPv6 range is invalid (start address higher than end address).";
		  break;
		} elseif ($v6range_from_array[$i] < $v6range_to_array[$i]) {
		break;
	  }
	}
	return $input_errors;
}
function format_bytes($bytes) {
	if ($bytes >= 1073741824) {
		return sprintf("%.2f GB", $bytes/1073741824);
	} else if ($bytes >= 1048576) {
		return sprintf("%.2f MB", $bytes/1048576);
	} else if ($bytes >= 1024) {
		return sprintf("%.0f KB", $bytes/1024);
	} else {
		return sprintf("%d bytes", $bytes);
	}
}

/* lock a file; decide that the lock is stale after $maxwait seconds */
function lock_file($lockfile, $maxwait = 20) {
	$n = 0;
	
	while ($n < $maxwait) {
		/* open the lock file in append mode to avoid race condition */
		if ($fd = @fopen($lockfile, "x")) {
			/* succeeded */
			fclose($fd);
			return;
		} else {
			/* file locked, wait and try again */
			usleep(rand(200000,500000));
			$n++;
		}
	}
}

function unlock_file($lockfile) {
	if (file_exists($lockfile))
		unlink($lockfile);
}

?>