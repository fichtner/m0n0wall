<?php
/*
	$Id$
	part of m0n0wall (http://m0n0.ch/wall)
	
	Copyright (C) 2003-2014 Manuel Kasper <mk@neon1.net>.
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
	
/* include all configuration functions */
require_once("functions.inc");
	
function vpn_ipsec_configure($ipchg = false) {
	global $config, $g;
	
	$curwanip = get_current_wan_address();
	$curwanip6 = get_current_wan_address(true);
	$syscfg = $config['system'];
	$ipseccfg = $config['ipsec'];
	$lancfg = $config['interfaces']['lan'];
	$lanip = $lancfg['ipaddr'];
	$lanip6 = $lancfg['ipaddr6'];
	$lansa = gen_subnet($lancfg['ipaddr'], $lancfg['subnet']);
	$lansa6 = gen_subnet6($lancfg['ipaddr6'], $lancfg['subnet6']);
	$lansn = $lancfg['subnet'];
	$lansn6 = $lancfg['subnet6'];
	$l2tpdcfg = $config['l2tp'];

	if ($g['booting']) {
		if (!isset($ipseccfg['enable']) && ($l2tpdcfg['mode'] != "server") )
			return 0;
		
		echo "Configuring IPsec VPN... ";
	} else {
		/* kill racoon and dnswatch */
		killbyname("racoon");
		killbypid("{$g['varrun_path']}/dnswatch-ipsec.pid");
		
		/* wait for racoon process to die */
		sleep(2);
		
		/* send a SIGKILL to be sure */
		sigkillbyname("racoon", "KILL");
	}
	
	/* flush SPD and SAD */
	mwexec("/usr/local/sbin/setkey -FP");
	mwexec("/usr/local/sbin/setkey -F");
	
	/* mark enc0 interface as up to enable filtering of decapsulated IPsec packets */
	mwexec("/sbin/ifconfig enc0 up");

	/* disable checksum for NATT */
	mwexec("/sbin/sysctl -w net.inet.esp.esp_ignore_natt_cksum=1");
	
	/* prefer new or old SAs? */
	if (!isset($ipseccfg['preferoldsa']))
		mwexec("/sbin/sysctl -w net.key.preferred_oldsa=0");
	else
		mwexec("/sbin/sysctl -w net.key.preferred_oldsa=1");
	
	if (isset($ipseccfg['enable']) || ($l2tpdcfg['mode'] == "server") ) {
		
		if (!$curwanip) {
			/* IP address not configured yet, exit */
			if ($g['booting'])
				echo "done\n";
			return 0;
		}
		
		if ((is_array($ipseccfg['tunnel']) && count($ipseccfg['tunnel'])) ||
				isset($ipseccfg['mobileclients']['enable']) || ($l2tpdcfg['mode'] == "server")) {
		
			$dnswatch_list = array();
			$rgmap = array();
		
			if (is_array($ipseccfg['tunnel']) && count($ipseccfg['tunnel']) || ($l2tpdcfg['mode'] == "server") ) {
			
				/* generate spd.conf */
				$fd = fopen("{$g['varetc_path']}/spd.conf", "w");
				if (!$fd) {
					printf("Error: cannot open spd.conf in vpn_ipsec_configure().\n");
					return 1;
				}
					
				$spdconf = "";

				/* add ipsec/l2tp spd */
				if ($l2tpdcfg['mode'] == "server") {
					$spdconf .= "spdadd 0.0.0.0/0[0] 0.0.0.0/0[1701] udp -P in  ipsec esp/transport//require;\n";
					$spdconf .= "spdadd 0.0.0.0/0[1701] 0.0.0.0/0[0] udp -P out ipsec esp/transport//require;\n";
				}

				if (isset($ipseccfg['enable']) && is_array($ipseccfg['tunnel']) && count($ipseccfg['tunnel'])) {
					if (ipv6enabled()) {
						$spdconf .= "spdadd {$lansa6}/{$lansn6} {$lanip6}/128 any -P in none;\n";
						$spdconf .= "spdadd {$lanip6}/128 {$lansa6}/{$lansn6} any -P out none;\n";
					} 
					$spdconf .= "spdadd {$lansa}/{$lansn} {$lanip}/32 any -P in none;\n";
					$spdconf .= "spdadd {$lanip}/32 {$lansa}/{$lansn} any -P out none;\n";
					
					foreach ($ipseccfg['tunnel'] as $tunnel) {
					
						if (isset($tunnel['disabled']))
							continue;
					
						/* see if this tunnel has a hostname for the remote-gateway, and if so,
						   try to resolve it now and add it to the list for dnswatch */
						if (!is_ipaddr4or6($tunnel['remote-gateway'])) {
							$dnswatch_list[] = $tunnel['remote-gateway'];
							$rgip = resolve_retry($tunnel['remote-gateway']);
							
							if (!$rgip)
								continue;
							
						} else {
							$rgip = $tunnel['remote-gateway'];
						}
						$rgmap[$tunnel['remote-gateway']] = $rgip;
						if (ipv6enabled() && (is_ipaddr6($rgip))) {
							$ep = vpn_endpoint_determine($tunnel, $curwanip6,$rgip);
						} else {
							$ep = vpn_endpoint_determine($tunnel, $curwanip,$rgip);
						}
						
						if (!$ep)
							continue;
						
						vpn_localnet_determine($tunnel['local-subnet'], $sa, $sn, $rgip);
						
						$spdconf .= "spdadd {$sa}/{$sn} " .
						"{$tunnel['remote-subnet']} any -P out ipsec " . 
						"{$tunnel['p2']['protocol']}/tunnel/{$ep}-" .
						"{$rgip}/unique;\n";
							
						$spdconf .= "spdadd {$tunnel['remote-subnet']} " .
						" {$sa}/{$sn} any -P in ipsec " .
						"{$tunnel['p2']['protocol']}/tunnel/{$rgip}-" .
							"{$ep}/unique;\n";
					}
				}
				
				fwrite($fd, $spdconf);
				fclose($fd);
			
				/* load SPD */
				mwexec("/usr/local/sbin/setkey -c < {$g['varetc_path']}/spd.conf");
			}
			
			/* generate racoon.conf */
			$fd = fopen("{$g['varetc_path']}/racoon.conf", "w");
			if (!$fd) {
				printf("Error: cannot open racoon.conf in vpn_ipsec_configure().\n");
				return 1;
			}
				
			$racoonconf = "path pre_shared_key \"{$g['varetc_path']}/psk.txt\";\n\n";
			$racoonconf .= "path certificate  \"{$g['varetc_path']}\";\n\n";
			
			/* generate CA certificates files */
			$cacertnum = 0;
			if (is_array($ipseccfg['cacert']) && count($ipseccfg['cacert']))
				foreach ($ipseccfg['cacert'] as $cacert) {
					++$cacertnum;
					if (isset($cacert['cert'])) {
						$cert = base64_decode($cacert['cert']);
						$x509cert = openssl_x509_parse(openssl_x509_read($cert));
						if(is_array($x509cert) && isset($x509cert['hash'])) {
							$fd1 = fopen("{$g['varetc_path']}/{$x509cert['hash']}.0", "w");
							if (!$fd1) {
								printf("Error: cannot open {$x509cert['hash']}.0 in vpn.\n");
								return 1;
							}
							chmod("{$g['varetc_path']}/{$x509cert['hash']}.0", 0600);
							fwrite($fd1, $cert);
							fclose($fd1);
							/* check for CRL for this CA */
							if (isset($cacert['crl'])) {
								$crl = base64_decode($cacert['crl']);
								$fd1 = fopen("{$g['varetc_path']}/{$x509cert['hash']}.r0", "w");
								if (!$fd1) {
									printf("Error: cannot open {$x509cert['hash']}.r0 in vpn.\n");
									return 1;
								}
								chmod("{$g['varetc_path']}/{$x509cert['hash']}.r0", 0600);
								fwrite($fd1, $crl);
								fclose($fd1);
							}
						} /* if(is_array($x509cert) */
					}
				}
			if (isset($ipseccfg['enable'])) {
				$tunnelnumber = 0;
				if (is_array($ipseccfg['tunnel']) && count($ipseccfg['tunnel']))
					foreach ($ipseccfg['tunnel'] as $tunnel) {
					
					++$tunnelnumber;
				
					if (isset($tunnel['disabled']))
						continue;
					
					$rgip = $rgmap[$tunnel['remote-gateway']];
					if (!$rgip)
						continue;
					
					if (ipv6enabled() && (is_ipaddr6($rgip))) {
							$ep = vpn_endpoint_determine($tunnel, $curwanip6,$rgip);
					} else {
							$ep = vpn_endpoint_determine($tunnel, $curwanip,$rgip);
					}
					if (!$ep)
						continue;
				
					vpn_localnet_determine($tunnel['local-subnet'], $sa, $sn, $rgip);
				
					if (isset($tunnel['p1']['myident']['myaddress'])) {
						$myidentt = "address";
						$myident = $ep;
					} else if (isset($tunnel['p1']['myident']['address'])) {
						$myidentt = "address";
						$myident = $tunnel['p1']['myident']['address'];
					} else if (isset($tunnel['p1']['myident']['fqdn'])) {
						$myidentt = "fqdn";
						$myident = $tunnel['p1']['myident']['fqdn'];
					} else if (isset($tunnel['p1']['myident']['ufqdn'])) {
						$myidentt = "user_fqdn";
						$myident = $tunnel['p1']['myident']['ufqdn'];
					} else if (isset($tunnel['p1']['myident']['asn1dn'])) {
						$myidentt = "asn1dn";
						$myident = $tunnel['p1']['myident']['asn1dn'];
	 				}

					if (!($myidentt == "asn1dn" && $myident == "")) {
						$myident = " \"".$myident."\"";
					}
						
					$nattline = '';
					if (isset($tunnel['natt'])) {
						$nattline = "nat_traversal on;";
					}
					
					if (isset($tunnel['p1']['authentication_method'])) {
						$authmethod = $tunnel['p1']['authentication_method'];
					} else {$authmethod = 'pre_shared_key';}
					
					$certline = '';	
					
					if ($authmethod == 'rsasig') {
						if ($tunnel['p1']['cert'] && $tunnel['p1']['private-key']) {
							$cert = base64_decode($tunnel['p1']['cert']);
							$private_key = base64_decode($tunnel['p1']['private-key']);
						} else {
							/* null certificate/key */
							$cert = '';
							$private_key = '';
						}
						
						if ($tunnel['p1']['peercert']) 
							$peercert = base64_decode($tunnel['p1']['peercert']);
						else 
							$peercert = '';
						
						$fd1 = fopen("{$g['varetc_path']}/server{$tunnelnumber}-signed.pem", "w");
						if (!$fd1) {
							printf("Error: cannot open server{$tunnelnumber}-signed.pem in vpn.\n");
							return 1;
						}
						chmod("{$g['varetc_path']}/server{$tunnelnumber}-signed.pem", 0600);
						fwrite($fd1, $cert);
						fclose($fd1);
						
						$fd1 = fopen("{$g['varetc_path']}/server{$tunnelnumber}-key.pem", "w");
						if (!$fd1) {
							printf("Error: cannot open server{$tunnelnumber}-key.pem in vpn.\n");
							return 1;
						}
						chmod("{$g['varetc_path']}/server{$tunnelnumber}-key.pem", 0600);
						fwrite($fd1, $private_key);
						fclose($fd1);

						$certline = "certificate_type x509 \"server{$tunnelnumber}-signed.pem\" \"server{$tunnelnumber}-key.pem\";";
						
						if ($peercert!=''){
							$fd1 = fopen("{$g['varetc_path']}/peer{$tunnelnumber}-signed.pem", "w");
							if (!$fd1) {
								printf("Error: cannot open server{$tunnelnumber}-signed.pem in vpn.\n");
								return 1;
							}
							chmod("{$g['varetc_path']}/peer{$tunnelnumber}-signed.pem", 0600);
							fwrite($fd1, $peercert);
							fclose($fd1);		
							$certline .= <<<EOD
						
	peers_certfile "peer{$tunnelnumber}-signed.pem";
EOD;
						} 					
					}
					
					$dpddelay = 0;
					if ($tunnel['dpddelay'])
						$dpddelay = $tunnel['dpddelay'];
					
					$racoonconf .= <<<EOD
remote {$rgip} \{
	exchange_mode {$tunnel['p1']['mode']};
	my_identifier {$myidentt}{$myident};
	{$nattline}
	{$certline}
	peers_identifier address {$rgip};
	initial_contact on;
	support_proxy on;
	proposal_check obey;
	dpd_delay $dpddelay;

	proposal \{
		encryption_algorithm {$tunnel['p1']['encryption-algorithm']};
		hash_algorithm {$tunnel['p1']['hash-algorithm']};
		authentication_method {$authmethod};
		dh_group {$tunnel['p1']['dhgroup']};

EOD;
					if ($tunnel['p1']['lifetime'])
						$racoonconf .= "		lifetime time {$tunnel['p1']['lifetime']} secs;\n";
					
					$racoonconf .= "	}\n";
					
					if ($tunnel['p1']['lifetime'])
						$racoonconf .= "	lifetime time {$tunnel['p1']['lifetime']} secs;\n";
						
					$racoonconf .= "}\n\n";
					
					$p2ealgos = join(",", $tunnel['p2']['encryption-algorithm-option']);
					$p2halgos = join(",", $tunnel['p2']['hash-algorithm-option']);
					
					$racoonconf .= <<<EOD
sainfo address {$sa}/{$sn} any address {$tunnel['remote-subnet']} any \{
	encryption_algorithm {$p2ealgos};
	authentication_algorithm {$p2halgos};
	compression_algorithm deflate;

EOD;

					if ($tunnel['p2']['pfsgroup'])
						$racoonconf .= "	pfs_group {$tunnel['p2']['pfsgroup']};\n";
						
					if ($tunnel['p2']['lifetime'])
						$racoonconf .= "	lifetime time {$tunnel['p2']['lifetime']} secs;\n";
						
					$racoonconf .= "}\n\n";
				}
			}
			/* ipsec/l2tp */
			if ($l2tpdcfg['mode'] == "server") {
				$racoonconf .= <<<EOD
# start l2tp configuration
remote anonymous
\{
        exchange_mode    main;
        passive          on;
        proposal_check   obey;
        support_proxy    on;
        nat_traversal    on;
        ike_frag         on;
        dpd_delay        20;

        proposal
        \{
                encryption_algorithm  aes;
                hash_algorithm        sha1;
                authentication_method pre_shared_key;
                dh_group              modp1024;
        }

        proposal
        \{
                encryption_algorithm  3des;
                hash_algorithm        sha1;
                authentication_method pre_shared_key;
                dh_group              modp1024;
        }
}

sainfo anonymous
\{
        encryption_algorithm     aes,3des;
        authentication_algorithm hmac_sha1;
        compression_algorithm    deflate;
        pfs_group                modp1024;
}
# end l2tp configuration
EOD;
			}
			/* mobile clients? */
			if (isset($ipseccfg['mobileclients']['enable'])) {
				
				$tunnel = $ipseccfg['mobileclients'];
				
				if (isset($tunnel['p1']['myident']['myaddress'])) {
					$myidentt = "address";
					$myident = $curwanip;
				} else if (isset($tunnel['p1']['myident']['address'])) {
					$myidentt = "address";
					$myident = $tunnel['p1']['myident']['address'];
				} else if (isset($tunnel['p1']['myident']['fqdn'])) {
					$myidentt = "fqdn";
					$myident = $tunnel['p1']['myident']['fqdn'];
				} else if (isset($tunnel['p1']['myident']['ufqdn'])) {
					$myidentt = "user_fqdn";
					$myident = $tunnel['p1']['myident']['ufqdn'];
				} else if (isset($tunnel['p1']['myident']['asn1dn'])) {
					$myidentt = "asn1dn";
					$myident = $tunnel['p1']['myident']['asn1dn'];
 				}

				if (!($myidentt == "asn1dn" && $myident == "")) {
					$myident = " \"".$myident."\"";
				}
					
				$nattline = '';
				if (isset($tunnel['natt'])) {
					$nattline = "nat_traversal on;";
				}
				
				if (isset($tunnel['p1']['authentication_method'])) {
					$authmethod = $tunnel['p1']['authentication_method'];
				} else {$authmethod = 'pre_shared_key';}
				
				$certline = '';					
				if ($authmethod == 'rsasig') {
					if ($tunnel['p1']['cert'] && $tunnel['p1']['private-key']) {
						$cert = base64_decode($tunnel['p1']['cert']);
						$private_key = base64_decode($tunnel['p1']['private-key']);
					} else {
						/* null certificate/key */
						$cert = '';
						$private_key = '';
					}
					
					if ($tunnel['p1']['peercert']) 
						$peercert = base64_decode($tunnel['p1']['peercert']);
					else 
						$peercert = '';
					
					$fd1 = fopen("{$g['varetc_path']}/server-mobile{$tunnelnumber}-signed.pem", "w");
					if (!$fd1) {
						printf("Error: cannot open server-mobile{$tunnelnumber}-signed.pem in vpn.\n");
						return 1;
					}
					chmod("{$g['varetc_path']}/server-mobile{$tunnelnumber}-signed.pem", 0600);
					fwrite($fd1, $cert);
					fclose($fd1);
					
					$fd1 = fopen("{$g['varetc_path']}/server-mobile{$tunnelnumber}-key.pem", "w");
					if (!$fd1) {
						printf("Error: cannot open server-mobile{$tunnelnumber}-key.pem in vpn.\n");
						return 1;
					}
					chmod("{$g['varetc_path']}/server-mobile{$tunnelnumber}-key.pem", 0600);
					fwrite($fd1, $private_key);
					fclose($fd1);

					$certline = "certificate_type x509 \"server-mobile{$tunnelnumber}-signed.pem\" \"server-mobile{$tunnelnumber}-key.pem\";";
				}
				
				$dpddelay = 0;
				if ($tunnel['dpddelay'])
					$dpddelay = $tunnel['dpddelay'];
				
				$racoonconf .= <<<EOD
remote anonymous \{
	exchange_mode {$tunnel['p1']['mode']};
	my_identifier {$myidentt}{$myident};
	{$nattline}
	{$certline}
	initial_contact on;
	passive on;
	generate_policy on;
	support_proxy on;
	proposal_check obey;
	dpd_delay $dpddelay;

	proposal \{
		encryption_algorithm {$tunnel['p1']['encryption-algorithm']};
		hash_algorithm {$tunnel['p1']['hash-algorithm']};
		authentication_method {$authmethod};
		dh_group {$tunnel['p1']['dhgroup']};

EOD;
				if ($tunnel['p1']['lifetime'])
					$racoonconf .= "		lifetime time {$tunnel['p1']['lifetime']} secs;\n";
				
				$racoonconf .= "	}\n";
				
				if ($tunnel['p1']['lifetime'])
					$racoonconf .= "	lifetime time {$tunnel['p1']['lifetime']} secs;\n";
					
				$racoonconf .= "}\n\n";
				
				$p2ealgos = join(",", $tunnel['p2']['encryption-algorithm-option']);
				$p2halgos = join(",", $tunnel['p2']['hash-algorithm-option']);
				
				$racoonconf .= <<<EOD
sainfo anonymous \{
	encryption_algorithm {$p2ealgos};
	authentication_algorithm {$p2halgos};
	compression_algorithm deflate;

EOD;

				if ($tunnel['p2']['pfsgroup'])
					$racoonconf .= "	pfs_group {$tunnel['p2']['pfsgroup']};\n";
					
				if ($tunnel['p2']['lifetime'])
					$racoonconf .= "	lifetime time {$tunnel['p2']['lifetime']} secs;\n";
					
				$racoonconf .= "}\n\n";
			}
			
			fwrite($fd, $racoonconf);
			fclose($fd);
			
			/* generate psk.txt */
			$fd = fopen("{$g['varetc_path']}/psk.txt", "w");
			if (!$fd) {
				printf("Error: cannot open psk.txt in vpn_ipsec_configure().\n");
				return 1;
			}
			
			$pskconf = "";
			
			if (is_array($ipseccfg['tunnel'])) {
				foreach ($ipseccfg['tunnel'] as $tunnel) {
					if (isset($tunnel['disabled']))
						continue;
					$rgip = $rgmap[$tunnel['remote-gateway']];
					if (!$rgip)
						continue;
					$pskconf .= "{$rgip}	 {$tunnel['p1']['pre-shared-key']}\n";
				}
			}
			
			/* add PSKs for mobile clients */
			if (is_array($ipseccfg['mobilekey'])) {
				foreach ($ipseccfg['mobilekey'] as $key) {
					$pskconf .= "{$key['ident']}	{$key['pre-shared-key']}\n";
				}
			}
			
			/* add ipsec/l2tp PSK */
			if ($l2tpdcfg['mode'] == "server") {
				$pskconf .= "* {$l2tpdcfg['ipsec']['psk']}\n";
			}

			fwrite($fd, $pskconf);
			fclose($fd);
			chmod("{$g['varetc_path']}/psk.txt", 0600);
			
			/* start racoon */
			mwexec("/usr/local/sbin/racoon -d -f {$g['varetc_path']}/racoon.conf");
			
			/* start dnswatch, if necessary */
			if (count($dnswatch_list) > 0) {
				$interval = 60;
				if ($ipseccfg['dns-interval'])
					$interval = $ipseccfg['dns-interval'];
				
				$hostnames = "";
				foreach ($dnswatch_list as $dns)
					$hostnames .= " " . escapeshellarg($dns);
				
				mwexec("/usr/local/bin/dnswatch {$g['varrun_path']}/dnswatch-ipsec.pid $interval " . 
					escapeshellarg("/etc/rc.newipsecdns") . $hostnames);
			}
			
			if (is_array($ipseccfg['tunnel'])) {
				foreach ($ipseccfg['tunnel'] as $tunnel) {
					if (isset($tunnel['auto'])) {
						$remotehost = substr($tunnel['remote-subnet'],0,strpos($tunnel['remote-subnet'],"/"));
						if (ipv6enabled() && (is_ipaddr6($rgip))) {
							$srchost = vpn_endpoint_determine($tunnel, $curwanip6,$rgip);
						} else {
							$srchost = vpn_endpoint_determine($tunnel, $curwanip,$rgip);
						}
						if ($srchost)
							mwexec_bg("/sbin/ping -c 1 -S {$srchost} {$remotehost}");
					}
				}
			}
		}
	}
	
	if (!$g['booting']) {
		/* reload the filter */
		filter_configure();
	}
	
	if ($g['booting'])
		echo "done\n";
	
	return 0;
}

function vpn_pptpd_configure() {
	global $config, $g;
	
	$syscfg = $config['system'];
	$pptpdcfg = $config['pptpd'];
	
	if ($g['booting']) {
		if (!$pptpdcfg['mode'] || ($pptpdcfg['mode'] == "off"))
			return 0;
		
		echo "Configuring PPTP VPN service... ";
	} else {	
		/* kill mpd */
		killbypid("{$g['varrun_path']}/mpd-vpn.pid");
		
		/* wait for process to die */
		sleep(2);
		
		/* remove mpd.conf, if it exists */
		unlink_if_exists("{$g['varetc_path']}/mpd-vpn/mpd.conf");
		unlink_if_exists("{$g['varetc_path']}/mpd-vpn/mpd.secret");
	}
		
	/* make sure mpd-vpn directory exists */
	if (!file_exists("{$g['varetc_path']}/mpd-vpn"))
		mkdir("{$g['varetc_path']}/mpd-vpn");
		
	switch ($pptpdcfg['mode']) {
		
		case 'server':
			
			/* write mpd.conf */
			$fd = fopen("{$g['varetc_path']}/mpd-vpn/mpd.conf", "w");
			if (!$fd) {
				printf("Error: cannot open mpd.conf in vpn_pptpd_configure().\n");
				return 1;
			}
			
			$nunits = $pptpdcfg['nunits'];
			if (!$nunits)
				$nunits = 16;
			
			$startip = $pptpdcfg['remoteip'];
			$endip = long2ip(ip2long($pptpdcfg['remoteip']) + $nunits - 1);
			
			$mpdconf = <<<EOD
startup:

pptpd:
	set ippool add pool1 $startip $endip
	
	create bundle template B
	set iface disable on-demand
	set iface enable proxy-arp
	set iface enable tcpmssfix
	set iface idle 1800
	set iface up-script /usr/local/sbin/vpn-linkup
	set iface down-script /usr/local/sbin/vpn-linkdown
	
	set bundle enable crypt-reqd
	set bundle enable compression
	
	set ipcp yes vjcomp
	set ipcp ranges {$pptpdcfg['localip']} ippool pool1
	
	set ccp yes mppc
	set mppc yes e128
	set mppc yes stateless

EOD;
			
			if (!isset($pptpdcfg['req128'])) {
				$mpdconf .= <<<EOD
	set mppc yes e40
	set mppc yes e56

EOD;
			}
			
			$dnsservers = array();
			
			if (is_array($pptpdcfg['dnsserver']) && ($pptpdcfg['dnsserver'][0])) {
				$dnsservers = array_filter($pptpdcfg['dnsserver'], "is_ipaddr");
			} else if (isset($config['dnsmasq']['enable'])) {
				$dnsservers[] = $config['interfaces']['lan']['ipaddr'];
				if ($syscfg['dnsserver'][0] && is_ipaddr($syscfg['dnsserver'][0]))
					$dnsservers[] =  $syscfg['dnsserver'][0];
			} else if (is_array($syscfg['dnsserver']) && ($syscfg['dnsserver'][0])) {
				$dnsservers = array_slice(array_filter($syscfg['dnsserver'], "is_ipaddr"), 0, 2);
			}
			
			if (!empty($dnsservers))
				$mpdconf .= "	set ipcp dns " . join(" ", $dnsservers) . "\n";
			
			$mpdconf .= <<<EOD

	create link template L pptp
	set link action bundle B
	
	set link enable multilink
	set link yes acfcomp protocomp
	set link no pap chap
	set link enable chap-msv2 eap
	set link mtu 1460
	set link mru 1460
	set link keep-alive 10 60
	set link enable incoming

EOD;
			
			
			if (isset($pptpdcfg['radius']['enable'])) {
				$mpdconf .= <<<EOD
	set radius server {$pptpdcfg['radius']['server']} "{$pptpdcfg['radius']['secret']}"
	set radius retries 3
	set radius timeout 10
	set auth enable radius-auth
	set auth disable internal
	set eap enable radius-proxy

EOD;

				if (isset($pptpdcfg['radius']['accounting'])) {
					$mpdconf .= <<<EOD
	set auth enable radius-acct

EOD;
				}
			}

			fwrite($fd, $mpdconf);
			fclose($fd);
			
			/* write mpd.secret */
			$fd = fopen("{$g['varetc_path']}/mpd-vpn/mpd.secret", "w");
			if (!$fd) {
				printf("Error: cannot open mpd.secret in vpn_pptpd_configure().\n");
				return 1;
			}
			
			$mpdsecret = "";
			
			if (is_array($pptpdcfg['user'])) {
				foreach ($pptpdcfg['user'] as $user)
					$mpdsecret .= "{$user['name']} \"{$user['password']}\" {$user['ip']}\n";
			}

			fwrite($fd, $mpdsecret);
			fclose($fd);
			chmod("{$g['varetc_path']}/mpd-vpn/mpd.secret", 0600);
			
			/* fire up mpd */
			mwexec("/usr/local/sbin/mpd5 -b -d {$g['varetc_path']}/mpd-vpn -p {$g['varrun_path']}/mpd-vpn.pid pptpd");
			
			break;
			
		case 'redir':
			break;
	}
	
	if (!$g['booting']) {
		/* reload the filter and traffic shaper */
		filter_configure();
		shaper_configure();
	}
	
	if ($g['booting'])
		echo "done\n";
	
	return 0;
}

function vpn_ipsec_l2tp_configure() {
	global $config, $g;
	
	$syscfg = $config['system'];
	$l2tpdcfg = $config['l2tp'];
	
	if ($g['booting']) {
		if (!$l2tpdcfg['mode'] || ($l2tpdcfg['mode'] == "off"))
			return 0;
		
		echo "Configuring L2TP VPN service... ";
	} else {	
		/* kill mpd */
		killbypid("{$g['varrun_path']}/mpd-vpn-l2tp.pid");
		
		/* wait for process to die */
		sleep(2);
		
		/* remove mpd.conf, if it exists */
		unlink_if_exists("{$g['varetc_path']}/mpd-vpn-l2tp/mpd.conf");
		unlink_if_exists("{$g['varetc_path']}/mpd-vpn-l2tp/mpd.secret");
	}
		
	/* make sure mpd-vpn directory exists */
	if (!file_exists("{$g['varetc_path']}/mpd-vpn-l2tp"))
		mkdir("{$g['varetc_path']}/mpd-vpn-l2tp");
		
	switch ($l2tpdcfg['mode']) {
		
		case 'server':
			
			/* write mpd.conf */
			$fd = fopen("{$g['varetc_path']}/mpd-vpn-l2tp/mpd.conf", "w");
			if (!$fd) {
				printf("Error: cannot open mpd.conf in vpn_l2tpd_configure().\n");
				return 1;
			}
			
			$nunits = $l2tpdcfg['nunits'];
			if (!$nunits)
				$nunits = 16;
			
			$startip = $l2tpdcfg['remoteip'];
			$endip = long2ip(ip2long($l2tpdcfg['remoteip']) + $nunits - 1);
			
			$mpdconf = <<<EOD
startup:

l2tp:
	set ippool add pool2 $startip $endip
	
	create bundle template L2TP_T
	set iface name l2tp
	set iface disable on-demand
	set iface enable proxy-arp
	set iface enable tcpmssfix
	set iface idle 1800
	set iface up-script /usr/local/sbin/vpn-linkup
	set iface down-script /usr/local/sbin/vpn-linkdown
	
	set ipcp yes vjcomp
	set ipcp ranges {$l2tpdcfg['localip']} ippool pool2

EOD;
			
			$dnsservers = array();
			
			if (is_array($l2tpdcfg['dnsserver']) && ($l2tpdcfg['dnsserver'][0])) {
				$dnsservers = array_filter($l2tpdcfg['dnsserver'], "is_ipaddr");
			} else if (isset($config['dnsmasq']['enable'])) {
				$dnsservers[] = $config['interfaces']['lan']['ipaddr'];
				if ($syscfg['dnsserver'][0] && is_ipaddr($syscfg['dnsserver'][0]))
					$dnsservers[] =  $syscfg['dnsserver'][0];
			} else if (is_array($syscfg['dnsserver']) && ($syscfg['dnsserver'][0])) {
				$dnsservers = array_slice(array_filter($syscfg['dnsserver'], "is_ipaddr"), 0, 2);
			}
			
			if (!empty($dnsservers))
				$mpdconf .= "	set ipcp dns " . join(" ", $dnsservers) . "\n";
			
			$mpdconf .= <<<EOD

	create link template L l2tp
	set link action bundle L2TP_T
	
	set link yes acfcomp protocomp
	set link no pap 
	set link enable chap eap
	set link mtu 1460

	set link keep-alive 10 60
	set link enable incoming

EOD;
			
			
			if (isset($l2tpdcfg['radius']['enable'])) {
				$mpdconf .= <<<EOD
	set radius server {$l2tpdcfg['radius']['server']} "{$l2tpdcfg['radius']['secret']}"
	set radius retries 3
	set radius timeout 10
	set auth enable radius-auth
	set auth disable internal
	set eap enable radius-proxy

EOD;

				if (isset($l2tpdcfg['radius']['accounting'])) {
					$mpdconf .= <<<EOD
	set auth enable radius-acct

EOD;
				}
			}

			fwrite($fd, $mpdconf);
			fclose($fd);
			
			/* write mpd.secret */
			$fd = fopen("{$g['varetc_path']}/mpd-vpn-l2tp/mpd.secret", "w");
			if (!$fd) {
				printf("Error: cannot open mpd.secret in vpn_l2tpd_configure().\n");
				return 1;
			}
			
			$mpdsecret = "";
			
			if (is_array($l2tpdcfg['user'])) {
				foreach ($l2tpdcfg['user'] as $user)
					$mpdsecret .= "{$user['name']} \"{$user['password']}\" {$user['ip']}\n";
			}

			fwrite($fd, $mpdsecret);
			fclose($fd);
			chmod("{$g['varetc_path']}/mpd-vpn-l2tp/mpd.secret", 0600);
			
			/* fire up mpd */
			mwexec("/usr/local/sbin/mpd5 -b -d {$g['varetc_path']}/mpd-vpn-l2tp -p {$g['varrun_path']}/mpd-vpn-l2tp.pid l2tp");
			
			break;
			
		case 'redir':
			break;
	}
	
	if (!$g['booting']) {
		/* reload the filter and traffic shaper */
		filter_configure();
		shaper_configure();
		vpn_ipsec_configure();
	}
	
	if ($g['booting'])
		echo "done\n";
	
	return 0;
}


function vpn_localnet_determine($adr, &$sa, &$sn,$rgip) {
	global $config, $g;

	if (isset($adr)) {
		if ($adr['network']) {			
			switch ($adr['network']) {
				case 'lan':
					if (ipv6enabled() && (is_ipaddr6($rgip))) {
						$sn = $config['interfaces']['lan']['subnet6'];
						$sa = gen_subnet6($config['interfaces']['lan']['ipaddr6'], $sn);
						break;
					} else {
						$sn = $config['interfaces']['lan']['subnet'];
						$sa = gen_subnet($config['interfaces']['lan']['ipaddr'], $sn);
						break;
					}
			}
		} else if ($adr['address']) {
			list($sa,$sn) = explode("/", $adr['address']);
			if (is_null($sn))
				$sn = 32;
		}
	} else {
		$sn = $config['interfaces']['lan']['subnet'];
		$sa = gen_subnet($config['interfaces']['lan']['ipaddr'], $sn);
	}
}

function vpn_endpoint_determine($tunnel, $curwanip,$rgip) {
	
	global $g, $config;
	
	if ((!$tunnel['interface']) || ($tunnel['interface'] == "wan")) {
		if ($curwanip)
			return $curwanip;
		else
			return null;
	} else if ($tunnel['interface'] == "lan") {
		if (ipv6enabled() && (is_ipaddr6($rgip))) {
				return $config['interfaces']['lan']['ipaddr6'];
			} else {
				return $config['interfaces']['lan']['ipaddr'];
			}
	} else {
		$oc = $config['interfaces'][$tunnel['interface']];
		
		if (isset($oc['enable']) && $oc['if']) {
			if (ipv6enabled() && (is_ipaddr6($rgip))) {
				return $oc['ipaddr6'];
			} else {
				return $oc['ipaddr'];
			}
		}
	}
	
	return null;
}
	
?>
