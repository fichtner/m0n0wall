<?php
/*
	$Id$
	part of m0n0wall (http://m0n0.ch/wall)
	
	Copyright (C) 2003-2008 Manuel Kasper <mk@neon1.net>.
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

function interfaces_secondaries_configure($if) {
	global $config, $g;
	$ifcfg = $config['interfaces'][$if];
	if (is_array($config['secondaries']['secondary'])) {
		$a_secondaries = &$config['secondaries']['secondary'];
		foreach ($a_secondaries as $secondary) {
			if ($if == $secondary['if']) {
				if (!ipv6enabled() && is_ipaddr6($secondary['ipaddr'])){
					continue;
				}
				if (ipv6enabled() && is_ipaddr6($secondary['ipaddr'])){
					$ifconfig = $config['interfaces'][$if]['if'] . " inet6 ";
				}
				if (is_ipaddr($secondary['ipaddr'])){
					$ifconfig = $config['interfaces'][$if]['if'] . " inet ";
				}
				$ifconfig .=  $secondary['ipaddr'] . "/" . $secondary['prefix']  . " alias";
				mwexec("/sbin/ifconfig " . $ifconfig);
			}
		}
	}
}

function interfaces_loopback_configure() {
	global $config, $g;

	mwexec("/sbin/ifconfig lo0 127.0.0.1");
	
	return 0;
}

function interfaces_vlan_configure() {
	global $config, $g;
	
	if (is_array($config['vlans']['vlan']) && count($config['vlans']['vlan'])) {
		
		$i = 0;
		
		foreach ($config['vlans']['vlan'] as $vlan) {
			
			$cmd = "/sbin/ifconfig vlan{$i} create vlan " . 
				escapeshellarg($vlan['tag']) . " vlandev " . 
				escapeshellarg($vlan['if']);
			
			mwexec($cmd);
			
			/* make sure the parent interface is up */
			mwexec("/sbin/ifconfig " . escapeshellarg($vlan['if']) . " up");
			
			$i++;
		}
	}
	
	return 0;
}

function interfaces_lan_configure() {
	global $config, $g;
	
	if ($g['booting'])
		echo "Configuring LAN interface... ";
	
	$lancfg = $config['interfaces']['lan'];
		
	/* wireless configuration? */
	if (is_array($lancfg['wireless']))
		interfaces_wireless_configure($lancfg['if'], $lancfg['wireless']);
	
	/* MAC spoofing? */
	if ($lancfg['spoofmac'])
		mwexec("/sbin/ifconfig " . escapeshellarg($lancfg['if']) . 
			" link " . escapeshellarg($lancfg['spoofmac']));
	
	/* media */
	if ($lancfg['media'] || $lancfg['mediaopt']) {
		$cmd = "/sbin/ifconfig " . escapeshellarg($lancfg['if']);
		if ($lancfg['media'])
			$cmd .= " media " . escapeshellarg($lancfg['media']);
		if ($lancfg['mediaopt'])
			$cmd .= " mediaopt " . escapeshellarg($lancfg['mediaopt']);
		mwexec($cmd);
	}
	
	$addflags = "";
	if (strpos($lancfg['if'], "fxp") !== false)
		$addflags .= " link0";

	if (isset($config['system']['polling']))
		$addflags .= " polling";
	
	mwexec("/sbin/ifconfig " . escapeshellarg($lancfg['if']) . " " . 
		escapeshellarg($lancfg['ipaddr'] . "/" . $lancfg['subnet']) . $addflags);
	
	interfaces_lan_configure6();
	
	if (!$g['booting']) {
		/* make new hosts file */
		system_hosts_generate();
		
		/* reconfigure static routes (kernel may have deleted them) */
		system_routing_configure();
		
		/* reload ipfilter (address may have changed) */
		filter_configure();
		
		/* reload shaper (subnet may have changed) */
		shaper_configure();
		
		/* reload IPsec tunnels */
		vpn_ipsec_configure();
		
		/* reload dhcpd (gateway may have changed) */
		services_dhcpd_configure();
		
		/* reload dhcp6s (gateway may have changed) */
		services_dhcp6s_configure();
		
		/* reload dnsmasq */
		services_dnsmasq_configure();
		
		/* reload webgui */
		system_webgui_start();
		
		/* reload captive portal */
		captiveportal_configure();
	}
	
	if ($g['booting'])
		echo "done\n";
	
	return 0;
}

function interfaces_lan_configure6() {
	global $config, $g;
	
	$lancfg = $config['interfaces']['lan'];
	remove_all_ipv6_addresses($lancfg['if']);
	mwexec('/sbin/ifconfig ' . $lancfg['if'] . ' inet6 -accept_rtadv');
	if (ipv6enabled()) {
		if ($lancfg['ipaddr6'] == "6to4") {
			/* take second /64 from 6to4 prefix on WAN interface */
			$v6addr = calc_6to4_address("lan");
			if ($v6addr)
				mwexec("/sbin/ifconfig " . escapeshellarg($lancfg['if']) . " inet6 $v6addr prefixlen 64 alias");
		} else if ($lancfg['ipaddr6'] == "DHCP-PD") {
			services_dhcp6c_configure();
		} else if ($lancfg['ipaddr6'] && $lancfg['subnet6']) {
			mwexec('/sbin/ifconfig ' . escapeshellarg($lancfg['if']) .
				' inet6 ' . escapeshellarg($lancfg['ipaddr6']) .
				' prefixlen ' . escapeshellarg($lancfg['subnet6'] . ' alias')
			);
		}
	}
	interfaces_secondaries_configure('lan');
	
	if (!$g['booting']) {
		/* reload rtadvd */
		interfaces_rtadvd_configure();
	}
}

function interfaces_optional_configure() {
	global $config, $g;
	
	for ($i = 1; isset($config['interfaces']['opt' . $i]); $i++) {
		interfaces_optional_configure_if($i);
	}
	
	interfaces_optional_configure6();
	
	if (!$g['booting']) {
		/* reconfigure static routes (kernel may have deleted them) */
		system_routing_configure();
		
		/* reload ipfilter (address may have changed) */
		filter_configure();
		
		/* reload shaper (address may have changed) */
		shaper_configure();
		
		/* reload IPsec tunnels */
		vpn_ipsec_configure();
		
		/* reload dhcpd (interface enabled/disabled/bridged status may have changed) */
		services_dhcpd_configure();
		
		/* reload dhcp6s (gateway may have changed) */
		services_dhcp6s_configure();
		
		/* restart dnsmasq */
		services_dnsmasq_configure();
	}
	
	return 0;
}

function interfaces_bridge_configure() {
	global $config, $g;
	// Setup the bridge sysctls to filter on member interfaces and not bridge itself
	mwexec("/sbin/sysctl net.link.bridge.pfil_member=1");
	mwexec("/sbin/sysctl net.link.bridge.pfil_bridge=0");
	$bridge_if_num = -1;
	$bridgearray = array();
	$iflist = array("lan", "wan");
	for ($i = 1; isset($config['interfaces']['opt' . $i]); $i++)
		if(isset($config['interfaces']['opt' . $i]['enable']))
			$iflist['opt' . $i] = 'opt' . $i;
	foreach($iflist as $if) {
		// go through all interfaces and check for bridged interfaces
		if ($config['interfaces'][$if]['bridge']) {
			// this interface is bridged
			$bridgefound = false;
			if ($bridge_if_num == -1) {
				// this is the first bridge, create the bridge0 interface
				$bridge_if_num++;
				$bridgearray[$bridge_if_num][] = convert_friendly_interface_to_real_interface_name($if);
				$bridgearray[$bridge_if_num][] = convert_friendly_interface_to_real_interface_name($config['interfaces'][$if]['bridge']);
			} else {
				// additional bridge
				// check if interface exists in another bridge
				// since the same int can't be added to two bridges
				$realif = convert_friendly_interface_to_real_interface_name($if);
				$realifbridge = convert_friendly_interface_to_real_interface_name($config['interfaces'][$if]['bridge']);
				for($x=0; $x <= $bridge_if_num; $x++) {
					if (in_array($realif, $bridgearray[$x])) {
						// the interface is already in this bridge
						$bridgefound = true;
						if (!in_array($realifbridge, $bridgearray[$x])) {
							// bridged interface isn't already in this
							// bridge, add it.
							$bridgearray[$bridge_if_num][] = $realifbridge;
						}
					} else {
						// see if the other member interface of the bridge is in existing bridge
						if (in_array($realifbridge, $bridgearray[$x])) {
							$bridgefound = true;
							if (!in_array($realif, $bridgearray[$x])) {
								// if real interface isn't already in
								// this bridge, add it.
								$bridgearray[$bridge_if_num][] = $realif;
							}
						}
					}
				}
				if (!$bridgefound) {
					// the interfaces of this bridge are not contained
					// in any existing bridge. Add new bridge interface.
					$bridge_if_num++;
					$bridgearray[$bridge_if_num][] = convert_friendly_interface_to_real_interface_name($if);
					$bridgearray[$bridge_if_num][] = convert_friendly_interface_to_real_interface_name($config['interfaces'][$if]['bridge']);
				}
			}
		}
	}
	// at this point, $bridgearray is fully populated for the number of bridges the system requires.
	// time to set them up.
	for($x=0; $x <= $bridge_if_num; $x++) {
		// destroy and create ifbridge interface
		mwexec("/sbin/ifconfig bridge{$x} destroy");
		mwexec("/sbin/ifconfig bridge{$x} create");
		// log commands run for debugging in /tmp
		$fd = fopen("{$g['tmp_path']}/bridge{$x}_config", "w");
		fwrite($fd, "/sbin/ifconfig bridge{$x} destroy \n");
		fwrite($fd, "/sbin/ifconfig bridge{$x} create \n");
		$bridgecmd = "/sbin/ifconfig bridge{$x} ";
		$lowestmtu = "1500";
		
		foreach ($bridgearray[$x] as $bridgeif) {
			// iterate through all the interfaces in this bridge
			// append to the bridgecmd for this interface
			$bridgecmd .= " addm $bridgeif ";
			// get MTU for this interface
			$mtu = get_interface_mtu($bridgeif);
			if (intval($mtu) == 0) {
				syslog(LOG_ERR, "An error occurred while trying to obtain the MTU setting for $bridgeif.  Using 1500.");
				$mtu = "1500";
			} else {
				if (intval($mtu) < intval($lowestmtu))
					$lowestmtu = $mtu;
			}
		}
		
		// force all bridged interfaces to same MTU, the lowest common denominator
		foreach ($bridgearray[$x] as $bridgeif) {
			mwexec("/sbin/ifconfig {$bridgeif} mtu {$lowestmtu}");
			fwrite($fd, "/sbin/ifconfig {$bridgeif} mtu {$lowestmtu} \n");
		}
		
		// bring up each interface
		foreach ($bridgearray[$x] as $bridgeif) {
			mwexec("/sbin/ifconfig {$bridgeif} up");
			fwrite($fd, "/sbin/ifconfig {$bridgeif} up \n");
		}
		
		// setup the bridge
		mwexec("{$bridgecmd}");
		fwrite($fd, "{$bridgecmd} \n");
		
		// in FreeBSD 6.x, without a final separate "up", it won't work
		mwexec("/sbin/ifconfig bridge{$x} up");
		fwrite($fd, "/sbin/ifconfig bridge{$x} up");
		
		fclose($fd);
	}
}

function interfaces_optional_configure6() {
	global $config, $g;
	
	for ($i = 1; isset($config['interfaces']['opt' . $i]); $i++) {
		interfaces_optional_configure_if6($i);
	}
	
	if (!$g['booting']) {
		/* reload rtadvd */
		interfaces_rtadvd_configure();
	}
	
	return 0;
}

function interfaces_optional_configure_if($opti) {
	global $config, $g;
	global $bridgeconfig;
	
	$optcfg = $config['interfaces']['opt' . $opti];
	
	if ($g['booting']) {
		$optdescr = "";
		if ($optcfg['descr'])
			$optdescr = " ({$optcfg['descr']})";
		echo "Configuring OPT{$opti}{$optdescr} interface... ";
	}
	
	if (isset($optcfg['enable'])) {
		/* wireless configuration? */
		if (is_array($optcfg['wireless']))
			interfaces_wireless_configure($optcfg['if'], $optcfg['wireless']);
		
		/* MAC spoofing? */
		if ($optcfg['spoofmac'])
			mwexec("/sbin/ifconfig " . escapeshellarg($optcfg['if']) . 
				" link " . escapeshellarg($optcfg['spoofmac']));
		
		/* media */
		if ($optcfg['media'] || $optcfg['mediaopt']) {
			$cmd = "/sbin/ifconfig " . escapeshellarg($optcfg['if']);
			if ($optcfg['media'])
				$cmd .= " media " . escapeshellarg($optcfg['media']);
			if ($optcfg['mediaopt'])
				$cmd .= " mediaopt " . escapeshellarg($optcfg['mediaopt']);
			mwexec($cmd);
		}
	
		$addflags = "";
		if (strpos($optcfg['if'], "fxp") !== false)
			$addflags .= " link0";
		
		if (isset($config['system']['polling']))
			mwexec("/sbin/ifconfig " . escapeshellarg($optcfg['if']) .  " polling");
	
		/* bridged? */
		if ($optcfg['bridge']) {
			interfaces_bridge_configure();
		} else {
			mwexec("/sbin/ifconfig " . escapeshellarg($optcfg['if']) . " " . 
				escapeshellarg($optcfg['ipaddr'] . "/" . $optcfg['subnet']) . $addflags);
		}
		interfaces_secondaries_configure('opt' . $opti);
	} else {
		mwexec("/sbin/ifconfig " . escapeshellarg($optcfg['if']) . 
			" delete down");
	}
	
	if ($g['booting'])
		echo "done\n";
	
	return 0;
}

function interfaces_optional_configure_if6($opti) {
	global $config, $g;
	
	$optcfg = $config['interfaces']['opt' . $opti];
	
	if (isset($optcfg['enable'])) {
		remove_all_ipv6_addresses($optcfg['if']);
		mwexec('/sbin/ifconfig ' . $optcfg['if'] . ' inet6 -accept_rtadv');
		if (ipv6enabled()) {
			if ($optcfg['ipaddr6'] == "6to4") {
				$v6addr = calc_6to4_address("opt" . $opti);
				if ($v6addr)
					mwexec("/sbin/ifconfig " . escapeshellarg($optcfg['if']) . " inet6 $v6addr prefixlen 64 alias");
			} else if ($optcfg['ipaddr6'] == "DHCP-PD") {
					services_dhcp6c_configure();
			} else if ($optcfg['ipaddr6'] && $optcfg['subnet6']) {
				mwexec('/sbin/ifconfig ' . escapeshellarg($optcfg['if']) .
					' inet6 ' . escapeshellarg($optcfg['ipaddr6']) .
					' prefixlen ' . escapeshellarg($optcfg['subnet6'] . ' alias')
				);
			}
		interfaces_secondaries_configure( 'opt' . $opti );
		}
	}
	return 0;
}

function interfaces_wireless_configure($if, $wlcfg) {
	global $config, $g;
	
	/* wireless configuration */
	
	/* the mode/standard (11a/b/g) needs to be set separately */
	if ($wlcfg['standard']) {
		mwexec("/sbin/ifconfig " . escapeshellarg($if) . " mode " .
			escapeshellarg($wlcfg['standard']));
	}
	
	$ifcargs = escapeshellarg($if) . 
		" ssid " . escapeshellarg($wlcfg['ssid']) . " channel " . 
		escapeshellarg($wlcfg['channel']) . " ";
	
	switch ($wlcfg['mode']) {
		case 'hostap':
			$ifcargs .= "-mediaopt adhoc mediaopt hostap ";
			break;
		case 'ibss':
		case 'IBSS':
			if (strstr($if, "an"))
				$ifcargs .= "mediaopt adhoc ";
			else
				$ifcargs .= "-mediaopt hostap mediaopt adhoc ";
			break;
		case 'bss':
		case 'BSS':
			if (strstr($if, "an"))
				$ifcargs .= "-mediaopt adhoc ";
			else
				$ifcargs .= "-mediaopt hostap -mediaopt adhoc ";
			break;
	}
	
	$ifcargs .= "up";
	
	mwexec("/sbin/ifconfig " . $ifcargs);
	
	/* need to set AP-specific parameters in a separate ifconfig call */
	if ($wlcfg['mode'] == "hostap") {
		$ifcargs = escapeshellarg($if);
		
		if (isset($wlcfg['hidessid']))
			$ifcargs .= " hidessid";
		else
			$ifcargs .= " -hidessid";
			
		mwexec("/sbin/ifconfig " . $ifcargs);
	}
	
	/* configure encryption (if needed) */
	interfaces_wireless_configure_encryption($if, $wlcfg);
	
	return 0;
}

function interfaces_wireless_configure_encryption($if, $wlcfg) {
	global $config, $g;
	
	/* kill any running hostapd */
	killbypid("{$g['varrun_path']}/hostapd-$if.pid");
	unlink_if_exists("{$g['varetc_path']}/hostapd-$if.conf");
	
	/* killing hostapd leaves the interface in down state - make sure it's up again */
	mwexec("/sbin/ifconfig " . escapeshellarg($if) . " up");

	if ($wlcfg['wpa']['mode'] && $wlcfg['wpa']['mode'] != "none") {

		if ($wlcfg['mode'] == "hostap") {
			/* use WPA in hostap mode */
			$hostapdcfg = generate_hostapd_config($if, $wlcfg);
			
			/* write config to temporary file */
			$fd = fopen("{$g['varetc_path']}/hostapd-$if.conf", "w");
			if (!$fd) {
				echo("Error: cannot open hostapd-$if.conf in interfaces_wireless_configure_encryption().\n");
				return 1;
			}
	
			fwrite($fd, $hostapdcfg);
			fclose($fd);
			
			/* start hostapd */
			mwexec("/usr/sbin/hostapd -B -P {$g['varrun_path']}/hostapd-$if.pid {$g['varetc_path']}/hostapd-$if.conf");
		}
		
	} else {
		/* use WEP (or no encryption) */
	
		if (isset($wlcfg['wep']['enable']) && is_array($wlcfg['wep']['key'])) {
			$ifcargs = escapeshellarg($if) . " wepmode on";
			
			$i = 1;
			foreach ($wlcfg['wep']['key'] as $wepkey) {
				$ifcargs .= " wepkey " . escapeshellarg("{$i}:{$wepkey['value']}");
				if (isset($wepkey['txkey'])) {
					$ifcargs .= " weptxkey {$i}";
				}
				$i++;
			}
			mwexec("/sbin/ifconfig " . $ifcargs);
		} else {
			/* no encryption - make sure WEP is disabled */
			mwexec("/sbin/ifconfig " . escapeshellarg($if) . " wepmode off");
		}
	}
}

function interfaces_wan_configure() {
	global $config, $g;
	
	$wancfg = $config['interfaces']['wan'];
	
	if ($g['booting'])
		echo "Configuring WAN interface... ";
	else {
		/* kill dhclient */
		mwexec("/usr/bin/killall dhclient");
		
		/* kill PPPoE client (mpd) */
		killbypid("{$g['varrun_path']}/mpd.pid");
		
		/* wait for processes to die */
		sleep(2);
		
		unlink_if_exists("{$g['varetc_path']}/dhclient.conf");
		unlink_if_exists("{$g['varetc_path']}/mpd.conf");
		unlink_if_exists("{$g['varetc_path']}/mpd.links");
		unlink_if_exists("{$g['vardb_path']}/wanip");
		unlink_if_exists("{$g['varetc_path']}/nameservers.inet.conf");
		unlink_if_exists("{$g['varetc_path']}/nameservers.inet6.conf");
	}
	
	/* remove all addresses first */
	while (mwexec("/sbin/ifconfig " . escapeshellarg($wancfg['if']) . " -alias") == 0);
	mwexec("/sbin/ifconfig " . escapeshellarg($wancfg['if']) . " down");
	
	/* wireless configuration? */
	if (is_array($wancfg['wireless']))
		interfaces_wireless_configure($wancfg['if'], $wancfg['wireless']);
	
	if ($wancfg['spoofmac'])
		mwexec("/sbin/ifconfig " . escapeshellarg($wancfg['if']) . 
			" link " . escapeshellarg($wancfg['spoofmac']));
		
	/* media */
	if ($wancfg['media'] || $wancfg['mediaopt']) {
		$cmd = "/sbin/ifconfig " . escapeshellarg($wancfg['if']);
		if ($wancfg['media'])
			$cmd .= " media " . escapeshellarg($wancfg['media']);
		if ($wancfg['mediaopt'])
			$cmd .= " mediaopt " . escapeshellarg($wancfg['mediaopt']);
		mwexec($cmd);
	}
	
	$addflags = "";
	if (strpos($wancfg['if'], "fxp") !== false)
		$addflags .= " link0";
	
	if (isset($config['system']['polling']))
		mwexec("/sbin/ifconfig " . escapeshellarg($wancfg['if']) .  " polling") ;
	
	switch ($wancfg['ipaddr']) {
	
		case 'dhcp':
			interfaces_wan_dhcp_configure();
			break;
			
		case 'pppoe':
			interfaces_wan_pppoe_configure();
			break;
			
		case 'pptp':
			interfaces_wan_pptp_configure();
			break;
			
		default:
			if (isset($wancfg['ispointtopoint']) && $wancfg['pointtopoint']) {
				mwexec("/sbin/ifconfig " . escapeshellarg($wancfg['if']) . " " . 
					escapeshellarg($wancfg['ipaddr'] . "/" . $wancfg['subnet']) . 
					" " . escapeshellarg($wancfg['pointtopoint']) . $addflags . " up");
			} else {
				mwexec("/sbin/ifconfig " . escapeshellarg($wancfg['if']) . " " . 
					escapeshellarg($wancfg['ipaddr'] . "/" . $wancfg['subnet']) . $addflags);
			}
			
			/* install default route */
			mwexec("/sbin/route delete default");
			mwexec("/sbin/route add default " . escapeshellarg($wancfg['gateway']));
			
			/* resync ipfilter (done automatically for DHCP/PPPoE/PPTP) */
			filter_resync();
	}
	
	interfaces_wan_configure6();
	
	if (!$g['booting']) {
		/* reconfigure static routes (kernel may have deleted them) */
		system_routing_configure();
		
		/* reload ipfilter */
		filter_configure();
		
		/* reload shaper */
		shaper_configure();
		
		/* reload ipsec tunnels */
		vpn_ipsec_configure();
		
		/* restart ez-ipupdate */
		services_dyndns_configure();
		
		/* force DNS update */
		services_dnsupdate_process();
		
		/* restart dnsmasq */
		services_dnsmasq_configure();
	}
	
	if ($g['booting'])
		echo "done\n";
	
	return 0;
}

function interfaces_wan_configure6($newwanip = false) {
	global $config, $g;
	
	$wancfg = $config['interfaces']['wan'];
	$wanif = get_real_wan_interface(true);
	
	remove_all_ipv6_addresses($wancfg['if']);
	remove_all_ipv6_addresses("stf0");
	remove_all_ipv6_addresses("gif0");
	services_dhcp6c_stop();
	mwexec('/sbin/ifconfig ' . $wanif . ' inet6 -accept_rtadv');	

	/* kill any running aiccu */
	killbypid("{$g['varrun_path']}/aiccu.pid");
	
	if (ipv6enabled()) {
		if ($wancfg['ipaddr6'] == "6to4") {
			/* obtain current IPv4 address and make a 6to4 address out of it */
			$v6addr = calc_6to4_address("wan");
			
			if ($v6addr) {
				mwexec("/sbin/ifconfig stf0 create");
				mwexec("/sbin/ifconfig stf0 inet6 $v6addr prefixlen 16 alias");
				mwexec('/sbin/route delete -inet6 default');
		
				/* always use 6to4 anycast address 192.88.99.1 for now */
				mwexec('/sbin/route add -inet6 default 2002:c058:6301::');
			
				if (!$g['booting']) {
					/* a changed 6to4 prefix on WAN means we may also need to change the
					   IPv6 prefix on LAN/OPT */
					interfaces_lan_configure6();
					interfaces_optional_configure6();
				}
			}
		} else if ($wancfg['ipaddr6'] == "DHCP") {
			services_dhcp6c_configure();
			mwexec('/sbin/ifconfig ' . $wanif . ' inet6 accept_rtadv');

		} else if ($wancfg['ipaddr6'] == "ppp") {
			if ($newwanip) {
				/* called from up-script -> add IPv6 default route */
				mwexec('/sbin/route delete -inet6 default');
				mwexec('/sbin/route add -inet6 default -iface ' . $g['pppoe_interface']);
				mwexec('/sbin/ifconfig ' . $wanif . ' inet6 accept_rtadv');
			}
			if ($config['interfaces']['lan']['ipaddr6'] == "DHCP-PD") {
				services_dhcp6c_configure();
			}
		} else if ($wancfg['ipaddr6'] == "aiccu") {
			/* fire up AICCU, which will take care of setting the IP address
			   on gif0 and adding the default route */
			
			/* can only start AICCU once the IPv4 connectivity is
			   ready; otherwise it will exit immediately... */
			if ($newwanip || is_ipaddr($wancfg['ipaddr'])) {
				$aiccuconf = <<<EOD
username {$wancfg['aiccu']['username']}
password {$wancfg['aiccu']['password']}
tunnel_id {$wancfg['aiccu']['tunnelid']}
ipv6_interface gif0
daemonize true
automatic true
requiretls false
pidfile {$g['varrun_path']}/aiccu.pid
defaultroute true

EOD;

				$fd = fopen("{$g['varetc_path']}/aiccu.conf", "w");
				fwrite($fd, $aiccuconf);
				fclose($fd);
			
				mwexec("/usr/local/sbin/sixxs-aiccu start {$g['varetc_path']}/aiccu.conf");
			
				/* at this point, it can take anywhere from a millisecond to forever
				   until gif0 has been configured by AICCU. Unfortunately, the
				   "setupscript" option in aiccu.conf isn't actually implemented
				   under Unix, so to figure out when gif0 comes up, we need to
				   watch it with a little shell script... */
				$watchsh = <<<EOD
#!/bin/sh

while true; do
	/sbin/ifconfig gif0 > /dev/null 2>&1
	if [ $? = 0 ]; then
		sleep 1
		/etc/rc.newwanip6
		exit
	fi
	sleep 1
done

EOD;
				$fd = fopen("{$g['tmp_path']}/gifwatch.sh", "w");
				fwrite($fd, $watchsh);
				fclose($fd);
				chmod("{$g['tmp_path']}/gifwatch.sh", 0755);
			
				mwexec_bg("{$g['tmp_path']}/gifwatch.sh");
			}
		} else {
			if ($wancfg['ipaddr6'] && $wancfg['subnet6'] && ($wancfg['tunnel6'] || $wancfg['gateway6'])) {
				$wanif6 = $wancfg['if'];
				$gw6 = escapeshellarg($wancfg['gateway6']);
				
				/* use GIF tunnel? */
				if ($wancfg['tunnel6']) {
					$curwanip6 = get_current_wan_address(false);
					
					if ($curwanip6) {
						$wanif6 = "gif0";
						$gw6 = "-iface gif0";
						
						mwexec('/sbin/ifconfig gif0 create');
						mwexec('/sbin/ifconfig gif0 tunnel ' . escapeshellarg($curwanip6) . ' ' . escapeshellarg($wancfg['tunnel6']));
						mwexec('/sbin/ifconfig gif0 up');
					} else {
						$wanif6 = false;
					}
				}
				
				if ($wanif6) {
					mwexec('/sbin/ifconfig ' . escapeshellarg($wanif6) .
						' inet6 ' . escapeshellarg($wancfg['ipaddr6']) .
						' prefixlen ' . escapeshellarg($wancfg['subnet6'] . ' alias')
					);
					/* default route */
					mwexec('/sbin/route delete -inet6 default');
					mwexec('/sbin/route add -inet6 default ' . $gw6);
				}
			}
		}
	}
	if (!$g['booting']) {
		/* reload rtadvd */
		interfaces_rtadvd_configure();
	}
}

function interfaces_wan_dhcp_configure() {
	global $config, $g;
	
	$wancfg = $config['interfaces']['wan'];

	/* generate dhclient.conf */
	$fd = fopen("{$g['varetc_path']}/dhclient.conf", "w");
	if (!$fd) {
		echo("Error: cannot open dhclient.conf in interfaces_wan_dhcp_configure().\n");
		return 1;
	}
 
 	$dhclientconf = "";
 
 	if ($wancfg['dhcphostname']) {
		$dhclientconf .= <<<EOD
send dhcp-client-identifier "{$wancfg['dhcphostname']}";
interface "{$wancfg['if']}" {
	send host-name "{$wancfg['dhcphostname']}";
}

EOD;
	}

	fwrite($fd, $dhclientconf);
	fclose($fd);
	
	/* fire up dhclient - don't wait for the lease (-nw) */
	mwexec("/sbin/dhclient -nw -cf {$g['varetc_path']}/dhclient.conf -sf /sbin/dhclient-script " .
		escapeshellarg($wancfg['if']));
	
	return 0;
}

function interfaces_wan_dhcp_down() {
	global $g;
	mwexec("/sbin/dhclient -r -nw -cf {$g['varetc_path']}/dhclient.conf -sf /sbin/dhclient-script ");
	sleep(3);
	/*make sure dhclient is killed and remove pid*/
	mwexec("/usr/bin/killall dhclient");
	unlink_if_exists("{$g['varrun_path']}/dhclient.pid");
}

function interfaces_wan_dhcp_up() {
	interfaces_wan_dhcp_configure();

	sleep(10);
}

function interfaces_wan_pppoe_configure() {
	global $config, $g;
	
	$wancfg = $config['interfaces']['wan'];
	$pppoecfg = $config['pppoe'];
	
	/* generate mpd.conf */
	$fd = fopen("{$g['varetc_path']}/mpd.conf", "w");
	if (!$fd) {
		echo("Error: cannot open mpd.conf in interfaces_wan_pppoe_configure().\n");
		return 1;
	}
	
	$mtu = 1492;
	if ($pppoecfg['mtu'])
		$mtu = $pppoecfg['mtu'];
	
	$mpdconf = <<<EOD
startup:

pppoe:
	new -i ng0 pppoe pppoe
	set iface route default
	set iface disable on-demand
	set iface idle 0
	set iface up-script /usr/local/sbin/ppp-linkup
	set iface enable tcpmssfix

EOD;

	/* IPv6 enabled on WAN interface? */
	if ($wancfg['ipaddr6'] == "ppp") {
		$mpdconf .= "	set bundle enable ipv6cp\n";
	}
	
	$mpdconf .= <<<EOD
	set bundle disable multilink
	set bundle disable noretry
	set auth authname "{$pppoecfg['username']}"
	set auth password "{$pppoecfg['password']}"
	set link keep-alive 10 60
	set link max-redial 0
	set link no acfcomp protocomp
	set link disable pap chap
	set link accept chap
	set link mtu $mtu
	set ipcp yes vjcomp
	set ipcp ranges 0.0.0.0/0 0.0.0.0/0

EOD;

	if (isset($config['system']['dnsallowoverride'])) {
		$mpdconf .= <<<EOD
	set ipcp enable req-pri-dns

EOD;
		if (!isset($config['pppoe']['dnsnosec'])) {
			$mpdconf .= <<<EOD
	set ipcp enable req-sec-dns

EOD;
		}
	}

	$mpdconf .= "\topen\n";

	fwrite($fd, $mpdconf);
	fclose($fd);
	
	/* generate mpd.links */
	$fd = fopen("{$g['varetc_path']}/mpd.links", "w");
	if (!$fd) {
		echo("Error: cannot open mpd.links in interfaces_wan_pppoe_configure().\n");
		return 1;
	}
		
	$mpdconf = <<<EOD
pppoe:
	set phys type pppoe
	set pppoe iface {$wancfg['if']}
	set pppoe service "{$pppoecfg['provider']}"
	set pppoe enable originate
	set pppoe disable incoming

EOD;

	fwrite($fd, $mpdconf);
	fclose($fd);
	
	/* fire up mpd */
	mwexec("/usr/local/sbin/mpd4 -b -d {$g['varetc_path']} -p {$g['varrun_path']}/mpd.pid pppoe");
	
	return 0;
}

function interfaces_wan_pppoe_down() {
	global $g;
	sigkillbypid("{$g['varrun_path']}/mpd.pid", "SIGUSR2");

	sleep(3);
}

function interfaces_wan_pppoe_up() {
	global $g;
	sigkillbypid("{$g['varrun_path']}/mpd.pid", "SIGUSR1");

	sleep(3);
}

function interfaces_wan_pptp_configure() {
	global $config, $g;
	
	$wancfg = $config['interfaces']['wan'];
	$pptpcfg = $config['pptp'];
	
	/* generate mpd.conf */
	$fd = fopen("{$g['varetc_path']}/mpd.conf", "w");
	if (!$fd) {
		echo("Error: cannot open mpd.conf in interfaces_wan_pptp_configure().\n");
		return 1;
	}
		
	$mpdconf = <<<EOD
startup:

pptp:
	new -i ng0 pptp pptp
	set iface route default
	set iface disable on-demand
	set iface idle 0
	set iface up-script /usr/local/sbin/ppp-linkup

EOD;

	/* IPv6 enabled on WAN interface? */
	if ($wancfg['ipaddr6'] == "ppp") {
		$mpdconf .= "	set bundle enable ipv6cp\n";
	}
	
	$mpdconf .= <<<EOD
	set bundle disable multilink
	set bundle disable noretry
	set auth authname "{$pptpcfg['username']}"
	set auth password "{$pptpcfg['password']}"
	set link keep-alive 10 60
	set link max-redial 0
	set link no acfcomp protocomp
	set link disable pap chap
	set link accept chap
	set ipcp no vjcomp
	set ipcp ranges 0.0.0.0/0 0.0.0.0/0

EOD;
	
	if (isset($config['system']['dnsallowoverride'])) {
		$mpdconf .= <<<EOD
	set ipcp enable req-pri-dns
	set ipcp enable req-sec-dns

EOD;
	}
	
	$mpdconf .= "\topen\n";

	fwrite($fd, $mpdconf);
	fclose($fd);
	
	/* generate mpd.links */
	$fd = fopen("{$g['varetc_path']}/mpd.links", "w");
	if (!$fd) {
		echo("Error: cannot open mpd.links in interfaces_wan_pptp_configure().\n");
		return 1;
	}
		
	$mpdconf = <<<EOD
pptp:
	set phys type pptp
	set pptp enable originate outcall
	set pptp disable windowing
	set pptp self {$pptpcfg['local']}
	set pptp peer {$pptpcfg['remote']}

EOD;

	fwrite($fd, $mpdconf);
	fclose($fd);
	
	/* configure interface */
	mwexec("/sbin/ifconfig " . escapeshellarg($wancfg['if']) . " " . 
		escapeshellarg($pptpcfg['local'] . "/" . $pptpcfg['subnet']));
	
	/* fire up mpd */
	mwexec("/usr/local/sbin/mpd4 -b -d {$g['varetc_path']} -p {$g['varrun_path']}/mpd.pid pptp");
	
	return 0;
}

function interfaces_wan_pptp_down() {
	global $g;
	sigkillbypid("{$g['varrun_path']}/mpd.pid", "SIGUSR2");

	sleep(3);
}

function interfaces_wan_pptp_up() {
	global $g;
	sigkillbypid("{$g['varrun_path']}/mpd.pid", "SIGUSR1");

	sleep(3);
}

function interfaces_rtadvd_configure() {
	global $config, $g;

	/* kill any running rtadvd */
	killbyname('rtadvd');

	$wasrunning = file_exists("{$g['varrun_path']}/rtadvd.pid");

	if (!ipv6enabled())
		return 0;

	/* rtadvd enabled on any interfaces? */
	$rtadvdifs = '';
	$rtadvdconf = '';
	
	if (isset($config['interfaces']['lan']['ipv6ra'])) {
		$raflags = "";
		if (isset($config['interfaces']['lan']['ipv6ram'])) {
			$raflags .= "m";
		}
		if (isset($config['interfaces']['lan']['ipv6rao'])) {
			$raflags .= "o";
		}
		$rtadvdifs .= ' ' . $config['interfaces']['lan']['if'];
		$rtadvdconf .= $config['interfaces']['lan']['if'] . ":raflags=\"" . $raflags . "\":maxinterval#10:mininterval#4:prefixlen#";
		if ($config['interfaces']['lan']['ipv6mode'] == "DHCP-PD") {
			$rtadvdconf .= 64 - $config['interfaces']['lan']['slalen'];
		} else {
			$rtadvdconf .= $config['interfaces']['lan']['subnet6'];
		}
		if (isset($config['interfaces']['lan']['ipv6ramtu'])) {
			$rtadvdconf .= ":mtu#" . $config['interfaces']['lan']['ipv6ramtu'];
		}
		$rtadvdconf .= "\n";

	}
	
	if (isset($config['interfaces']['wan']['ipv6ra'])) {
		$wanif = get_real_wan_interface(true);
		$rtadvdifs .= ' ' . $wanif;
		$rtadvdconf .= $wanif . ":maxinterval#10:mininterval#4\n";
	}
	
	for ($i = 1; isset($config['interfaces']['opt' . $i]); $i++) {
		$optcfg = $config['interfaces']['opt' . $i];
		if (isset($optcfg['ipv6ra']) && isset($optcfg['enable']) &&
			$optcfg['if'] && !$optcfg['bridge']) {
			$raflags = "";
			if (isset($config['interfaces']['opt' . $i]['ipv6ram'])) {
				$raflags .= "m";
			}
			if (isset($config['interfaces']['opt' . $i]['ipv6rao'])) {
				$raflags .= "o";
			}
			$rtadvdifs .= ' ' . $optcfg['if'];
			/* maybe this can be configurable in the future */
			$rtadvdconf .= $optcfg['if'] . ":raflags=\"" . $raflags . "\":maxinterval#10:mininterval#4:prefixlen#";
			if ($config['interfaces']['opt' . $i]['ipv6mode'] == "DHCP-PD") {
				$rtadvdconf .= 64 - $config['interfaces']['opt' . $i]['slalen'];
			} else {
				$rtadvdconf .= $config['interfaces']['opt' . $i]['subnet6'];
			}
			if (isset($config['interfaces']['opt' . $i]['ipv6ramtu'])) {
				$rtadvdconf .= ":mtu#" . $config['interfaces']['opt' . $i]['ipv6ramtu'];
			}
			$rtadvdconf .= "\n";
		}
	}

	if (!$rtadvdifs)
		return 0;

	if ($g['booting'])
		echo 'Starting IPv6 router advertisement service... ';
	else if ($wasrunning) {
		/* rtadvd first sends out advertisements that
			there are no longer routes before effectively exiting
			(look at die() in rtadvd.c); this can take up to 10 seconds, but we
			don't want to wait that long - just give it time to send out one
			such advertisement */
		sleep(1);
		sigkillbyname('rtadvd', 'KILL');
	}

	/* this file is deleted here because we might create a race condition
		when deleting it earlier */
	unlink_if_exists("{$g['varrun_path']}/rtadvd.pid");

	/* write rtadvd.conf */
	$fd = fopen("{$g['varetc_path']}/rtadvd.conf", 'w');
	if (!$fd) {
		printf("Error: cannot open rtadvd.conf in interfaces_rtadvd_configure().\n");
		return 1;
	}
	fwrite($fd, $rtadvdconf);
	fclose($fd);

	/* fire up rtadvd */
	mwexec("/usr/sbin/rtadvd -D -c {$g['varetc_path']}/rtadvd.conf $rtadvdifs 2>&1 & " .
		"echo \"$!\" > {$g['varrun_path']}/rtadvd.pid");

	if (!$g['booting']) {
		filter_configure();
	} else
		echo "done\n";

	return 0;
}

function get_real_wan_interface($ipv6 = false) {
	global $config, $g;
	
	$wancfg = $config['interfaces']['wan'];
	
	$wanif = $wancfg['if'];
	
	if ($ipv6) {
		if ($wancfg['ipaddr6'] == "6to4")
			$wanif = "stf0";
		else if ($wancfg['ipaddr6'] == "aiccu" && !isset($config['interfaces']['wan']['aiccu']['ayiya']))
			$wanif = "gif0";
		else if ($wancfg['ipaddr6'] == "aiccu" && isset($config['interfaces']['wan']['aiccu']['ayiya']))
			$wanif = "tun0";
		else if ($wancfg['tunnel6'])
			$wanif = "gif0";
		else if ($wancfg['ipaddr6'] == "ppp")
			$wanif = $g['pppoe_interface'];
	} else {
		if (($wancfg['ipaddr'] == "pppoe") || ($wancfg['ipaddr'] == "pptp")) {
			$wanif = $g['pppoe_interface'];
		}
	}
	
	return $wanif;
}

function get_current_wan_address($ipv6 = false) {
	global $config, $g;
	
	$wancfg = $config['interfaces']['wan'];
	
	if (in_array($wancfg['ipaddr'], array('pppoe','dhcp','pptp')) || ($ipv6 && $wancfg['ipaddr6'] == "aiccu")) {
		/* dynamic WAN IP address, find out which one */
		$wanif = get_real_wan_interface($ipv6);
		
		if ($ipv6) {
			/* if the IPv4 address is dynamic, then the IPv6 address could also be
			   (in case we're using 6to4) */
			
			/* can't use netstat for IPv6, as it will always truncate addresses, even if -W is used :( */
			exec("/sbin/ifconfig " . escapeshellarg($wanif) . " 2>/dev/null", $ifinfo);
			
			foreach ($ifinfo as $line) {
				if (preg_match("/inet6 ([0-9a-f:]+) /", $line, $matches)) {
					if (is_ipaddr6($matches[1]))
						return $matches[1];
				}
			}
			
		} else {
			/* get interface info with netstat */
			exec("/usr/bin/netstat -nWI " . escapeshellarg($wanif) . " -f inet", $ifinfo);

			if (isset($ifinfo[1])) {
				$aif = preg_split("/\s+/", $ifinfo[1]);
				$curwanip = chop($aif[3]);

				if ($curwanip && is_ipaddr($curwanip) && ($curwanip != "0.0.0.0"))
					return $curwanip;
			}
		}
		
		return null;
	} else {
		/* static WAN IP address */
		return $wancfg[$ipv6 ? 'ipaddr6' : 'ipaddr'];
	}
}

function get_interface_mac($interface) {
  
        /* build interface list with netstat */
        exec("/usr/bin/netstat -I $interface -nW -f link", $linkinfo);
        array_shift($linkinfo);
        $alink = preg_split("/\s+/", $linkinfo[0]);
        $mac = chop($alink[3]);
        return $mac;
}

function generate_hostapd_config($if, $wlcfg) {
	$config = <<<EOD
interface=$if
driver=bsd
logger_syslog=-1
logger_syslog_level=2
logger_stdout=0
ssid={$wlcfg['ssid']}
wpa={$wlcfg['wpa']['version']}

EOD;
	
	if ($wlcfg['wpa']['mode'] == "psk") {
		$config .= <<<EOD
wpa_key_mgmt=WPA-PSK
wpa_passphrase={$wlcfg['wpa']['psk']}

EOD;
	} else if ($wlcfg['wpa']['mode'] == "enterprise") {
	
		$authport = 1812;
		$acctport = 1813;
		if ($wlcfg['wpa']['radius']['authport'])
			$authport = $wlcfg['wpa']['radius']['authport'];
		if ($wlcfg['wpa']['radius']['acctport'])
			$acctport = $wlcfg['wpa']['radius']['acctport'];
	
		$config .= <<<EOD
ieee8021x=1
wpa_key_mgmt=WPA-EAP
auth_server_addr={$wlcfg['wpa']['radius']['server']}
auth_server_port={$authport}
auth_server_shared_secret={$wlcfg['wpa']['radius']['secret']}
acct_server_addr={$wlcfg['wpa']['radius']['server']}
acct_server_port={$acctport}
acct_server_shared_secret={$wlcfg['wpa']['radius']['secret']}

EOD;
	}
	
	if ($wlcfg['wpa']['cipher'] == "tkip")
		$config .= "wpa_pairwise=TKIP\n";
	else if ($wlcfg['wpa']['cipher'] == "ccmp")
		$config .= "wpa_pairwise=CCMP\n";
	else
		$config .= "wpa_pairwise=TKIP CCMP\n";
	
	return $config;
}

function get_interface_ipv6_adresses($if) {
	$addresses = array();
	unset($ifc);
	exec('/sbin/ifconfig ' . escapeshellarg($if) . ' 2>/dev/null', $ifc);

	foreach ($ifc as $i) {
		if (preg_match('/inet6 (.+) prefixlen (\d+)/', $i, $matches)) {
			$ip6 = preg_replace('/%.*$/', '', trim($matches[1]));
			array_push($addresses, array($ip6, (int)$matches[2]));
		}
	}

	return $addresses;
}

function remove_all_ipv6_addresses($if) {
	$addresses = get_interface_ipv6_adresses($if);

	foreach ($addresses as $adr) {
		if (!preg_match('/^fe80:/', $adr[0])) {
			mwexec('/sbin/ifconfig ' . escapeshellarg($if) .
				' inet6 ' . escapeshellarg($adr[0]) . ' -alias');
		}
	}
}

function calc_6to4_address($if) {
	$wan_v6nibbles = convert_to_6to4_nibbles(get_current_wan_address(false));
	if ($wan_v6nibbles) {
		if ($if == "wan")
			$nth = 0;
		else if ($if == "lan")
			$nth = 1;
		else if (preg_match('/^opt(\d+)$/', $if, $matches))
			$nth = $matches[1] + 1;
		
		return "2002:{$wan_v6nibbles}:{$nth}::1";
	}
	return false;
}

function get_dhcppd_address($if) {
	$addresses = get_interface_ipv6_adresses($if);

	foreach ($addresses as $adr) {
		if ((!preg_match('/^fe80:/', $adr[0])) || (!preg_match('/autoconf$/',$adr[0]))) {
			$ipv6addr=$adr;
		}
	}
	return $ipv6addr;
	
}

function suggest_ipv6_lan_addr(){
	$sugipv6lan = "No RA found! Maybe try again in a few minutes.";
	 /* get the prefix */
	exec("/usr/sbin/clog /var/log/system.log", $logarr);
	
	foreach ($logarr as $logent) {
		$logent = preg_split("/\s+/", $logent, 6);
		$lograsrch = strstr($logent[5] , "<prefix_check> prefix ");
		if ($lograsrch) {
			$prefixpos = strpos($lograsrch, "/",0);
			$sugipv6lan = substr($lograsrch,21,$prefixpos -21);	
			$sugipv6lan .= "1" . substr($lograsrch,$prefixpos,strpos($lograsrch, "from",0)-$prefixpos);
		} 
	}
	if (!is_ipaddr6andprefix ($sugipv6lan) && ($sugipv6lan != "No RA found. Maybe try again in a few minutes.")) {
		return "Error calculating IPv6 address!";
	} else {
		return $sugipv6lan;
	}
}
?>
