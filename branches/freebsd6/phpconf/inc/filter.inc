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
	
/* include all configuration functions */
require_once("functions.inc");
	
function filter_resync() {
	global $config, $g;
	
	mwexec("/sbin/ipf -y");
	mwexec("/sbin/ipf -6 -y");
}

function filter_ipmon_start() {
	global $config, $g;
	
	mwexec("/sbin/ipmon -sD");
}

function filter_configure() {
	global $config, $g;
	
	if ($g['booting'])
		echo "Configuring firewall... ";
	
	/* set TCP timeouts */
	if ($config['filter']['tcpidletimeout'] && $g['booting']) {
		$tcpidletimeout = $config['filter']['tcpidletimeout']*2;
		
		/* ipfilter now needs to be disabled briefly in order to change the timeout value :( */
		mwexec("/sbin/ipf -D");
		mwexec("/sbin/sysctl net.inet.ipf.fr_tcpidletimeout={$tcpidletimeout}");
		mwexec("/sbin/ipf -E");
	}
	
	/* generate ipnat rules */
	$ipnatrules = filter_nat_rules_generate();
	
	/* load ipnat rules */
	$fd = popen("/sbin/ipnat -C -f - > /dev/null 2>&1", "w");
	if (!$fd) {
		printf("Cannot open /sbin/ipnat in filter_configure()\n");
		return 1;
	}
		
	fwrite($fd, $ipnatrules);
	pclose($fd);
	
	/* generate ipf rules */
	$ipfrules = filter_rules_generate();
	
	$fd = popen("/sbin/ipf -Fa -f - > /dev/null 2>&1", "w");
	if (!$fd) {
		printf("Cannot open /sbin/ipf in filter_configure()\n");
		return 1;
	}

	fwrite($fd, $ipfrules);
	pclose($fd);
	
	/* generate ipf rules for IPv6 */
	$ipfrules_ipv6 = filter_rules_generate_ipv6();
	$fd = popen("/sbin/ipf -6 -Fa -f - > /dev/null 2>&1", "w");
	if (!$fd) {
		printf("Cannot open /sbin/ipf in filter_configure()\n");
		return 1;
	}
	fwrite($fd, $ipfrules_ipv6);
	pclose($fd);
	
	if ($g['booting'])
		echo "done\n";
	
	return 0;
}

function filter_flush_nat_table() {
	global $config, $g;
	
	return mwexec("/sbin/ipnat -F");
}

function filter_flush_state_table() {
	global $config, $g;
	
	return mwexec("/sbin/ipf -FS");
}

function filter_flush_state_table_ipv6() {
	if (ipv6enabled()) {
		return mwexec("/sbin/ipf -6 -FS");
	} else {
		return 0;
	}
}

function filter_nat_rules_generate_if($if, $src, $dst, $target, $portmap = true) {

	if ($target)
		$tgt = $target . "/32";
	else
		$tgt = "0/32";
	
	$natrule = "map $if $src $dst -> {$tgt} proxy port ftp ftp/tcp\n";
	
	if ($portmap)
		$natrule .= "map $if $src $dst -> {$tgt} portmap tcp/udp auto\n";
	
	$natrule .= "map $if $src $dst -> {$tgt}\n";
	
	return $natrule;
}

function filter_nat_rules_generate() {
	global $config, $g;
	
	$wancfg = $config['interfaces']['wan'];
	$lancfg = $config['interfaces']['lan'];
	
	$pptpdcfg = $config['pptpd'];
	$wanif = get_real_wan_interface();
	
	$lansa = gen_subnet($lancfg['ipaddr'], $lancfg['subnet']);
	
	$natrules = "";
	
	/* any 1:1 mappings? */
	if (is_array($config['nat']['onetoone'])) {
		foreach ($config['nat']['onetoone'] as $natent) {
			if (!is_numeric($natent['subnet']))
				$sn = 32;
			else
				$sn = $natent['subnet'];
			
			if (!$natent['interface'] || ($natent['interface'] == "wan"))
				$natif = $wanif;
			else
				$natif = $config['interfaces'][$natent['interface']]['if'];
			
			$natrules .= "bimap {$natif} {$natent['internal']}/{$sn} -> {$natent['external']}/{$sn}\n";
		}
	}
	
	/* outbound rules - advanced or standard */
	if (isset($config['nat']['advancedoutbound']['enable'])) {
		/* advanced outbound rules */
		if (is_array($config['nat']['advancedoutbound']['rule'])) {
			foreach ($config['nat']['advancedoutbound']['rule'] as $obent) {				
				$dst = "";
				$src = "";
				if (!isset($obent['destination']['any'])) {
					$src = "from ";
					if (isset($obent['destination']['not'])) 
						$dst = "! to ";
					else
						$dst = "to ";
					$dst .= $obent['destination']['network'];
				}
				$src .= $obent['source']['network'];
				
				if (!$obent['interface'] || ($obent['interface'] == "wan"))
					$natif = $wanif;
				else
					$natif = $config['interfaces'][$obent['interface']]['if'];
				
				$natrules .= filter_nat_rules_generate_if($natif, $src, $dst,
					$obent['target'], !isset($obent['noportmap']));
			}
		}	
	} else {
		/* standard outbound rules (one for each interface) */
		$natrules .= filter_nat_rules_generate_if($wanif,
			$lansa . "/" . $lancfg['subnet'], "", null);
	
		/* optional interfaces */
		for ($i = 1; isset($config['interfaces']['opt' . $i]); $i++) {
			$optcfg = $config['interfaces']['opt' . $i];
			
			if (isset($optcfg['enable']) && !$optcfg['bridge']) {
				$optsa = gen_subnet($optcfg['ipaddr'], $optcfg['subnet']);
				$natrules .= filter_nat_rules_generate_if($wanif, 
					$optsa . "/" . $optcfg['subnet'], "", null);
			}
		}
		
		/* PPTP subnet */
		if ($pptpdcfg['mode'] == "server") {
			$natrules .= filter_nat_rules_generate_if($wanif,
				$pptpdcfg['remoteip'] . "/" . 
				subnet_size_to_cidr($pptpdcfg['nunits'] ? $pptpdcfg['nunits'] : 16),
				"", null);
		}
		
		/* static routes */
		if (is_array($config['staticroutes']['route'])) {
			foreach ($config['staticroutes']['route'] as $route) {
				if ($route['interface'] != "wan")
					$natrules .= filter_nat_rules_generate_if($wanif,
						$route['network'], "", null);
			}
		}
	}
	
	if (isset($config['nat']['rule'])) {
		foreach ($config['nat']['rule'] as $rule) {
		
			$extport = explode("-", $rule['external-port']);
			$target = alias_expand_host($rule['target']);
			
			if (!$target)
				continue;	/* unresolvable alias */
			
			if ($rule['external-address'])
				$extaddr = $rule['external-address'] . "/32";
			else
				$extaddr = "0/0";
			
			if (!$rule['interface'] || ($rule['interface'] == "wan"))
				$natif = $wanif;
			else
				$natif = $config['interfaces'][$rule['interface']]['if'];
			
			if ((!$extport[1]) || ($extport[0] == $extport[1])) {
				$natrules .=
					"rdr $natif {$extaddr} port {$extport[0]} -> {$target} " .
					"port {$rule['local-port']} {$rule['protocol']}";
			} else {
				$natrules .=
					"rdr $natif {$extaddr} port {$extport[0]}-{$extport[1]} " .
					"-> {$target} " .
					"port {$rule['local-port']} {$rule['protocol']}";
			}
				
			$natrules .= "\n";
		}
	}
	
	if ($pptpdcfg['mode'] == "redir" && $pptpdcfg['redir']) {
	
		$natrules .= <<<EOD

# PPTP
rdr $wanif 0/0 port 0 -> {$pptpdcfg['redir']} port 0 gre
rdr $wanif 0/0 port 1723 -> {$pptpdcfg['redir']} port 1723 tcp

EOD;
	}
	if (isset($config['sip']['enable']) && (5060 == $config['sip']['port'])) {
	
	    $lancfg = $config['interfaces'][$config['sip']['ifin']];
        $lanif = $lancfg['if'];
	    $lanip = $lancfg['ipaddr'];
	    $curwanip = get_current_wan_address();
		$natrules .= <<<EOD

# SIP transparent proxy
rdr $lanif 0/0 port 5060 -> $lanip port 5060 udp
rdr $wanif 0/0 port 5060 -> $curwanip port 5060 udp

EOD;
	}
	
	return $natrules;
}

function filter_rules_generate_ipv6() {
	global $config, $g;
	
	if (ipv6enabled() && $config['interfaces']['lan']['ipaddr6']) {
		return filter_rules_generate(true);
	} else {
		return <<<EOD
block in quick all
block out quick all

EOD;
	}
}

function filter_rules_generate($ipv6 = false) {
	global $config, $g;
	
	$wancfg = $config['interfaces']['wan'];
	$lancfg = $config['interfaces']['lan'];
	$pptpdcfg = $config['pptpd'];
	
	$lanif = $lancfg['if'];
	$wanif = get_real_wan_interface($ipv6);
	
	$curwanip = get_current_wan_address($ipv6);
	
	/* rule groups (optional interfaces: see below) */
	$ifgroups = array("lan" => 100, "wan" => 200);
	
	$lanip = $lancfg[$ipv6 ? 'ipaddr6' : 'ipaddr'];
	$lansn = $lancfg[$ipv6 ? 'subnet6' : 'subnet'];
	if ($ipv6) {
		if ($lanip == "6to4") {
			$lanip = calc_6to4_address("lan");
			$lansn = 64;
		}
		
		if (!$lanip) {
			$lanip = "::1";	/* dummy until 6to4 address is known */
			$lansa = "::";
		} else {
			$lansa = gen_subnet6($lanip, $lansn);
		}
	} else {
		$lansa = gen_subnet($lanip, $lansn);
	}
	
	/* optional interfaces */
	$optcfg = array();
	
	for ($i = 1; isset($config['interfaces']['opt' . $i]); $i++) {
		$oc = $config['interfaces']['opt' . $i];
		
		if (isset($oc['enable']) && $oc['if']) {
			$oic = array();
			$oic['if'] = $oc['if'];
			
			if ($oc['bridge']) {
				if (!strstr($oc['bridge'], "opt") || 
					 isset($config['interfaces'][$oc['bridge']]['enable'])) {
					if ($ipv6 ? is_ipaddr6($config['interfaces'][$oc['bridge']]['ipaddr6']) :
								is_ipaddr($config['interfaces'][$oc['bridge']]['ipaddr'])) {
						$oic['ip'] = $config['interfaces'][$oc['bridge']][$ipv6 ? 'ipaddr6' : 'ipaddr'];
						$oic['sn'] = $config['interfaces'][$oc['bridge']][$ipv6 ? 'subnet6' : 'subnet'];
						if ($ipv6)
							$oic['sa'] = gen_subnet6($oic['ip'], $oic['sn']);
						else
							$oic['sa'] = gen_subnet($oic['ip'], $oic['sn']);
					}
				}
				$oic['bridge'] = 1;
				$oic['bridge_if'] = $oc['bridge'];
			} else {
				if ($ipv6) {
					if ($oc['ipaddr6'] == "6to4") {
						$oic['ip'] = calc_6to4_address('opt' . $i);
						$oic['sn'] = 64;
					} else {
						$oic['ip'] = $oc['ipaddr6'];
						$oic['sn'] = $oc['subnet6'];
					}
					
					/* IPv6: ignore interface if no address is set or 6to4 address not yet known */
					if (!$oic['ip'])
						continue;
				} else {
					$oic['ip'] = $oc['ipaddr'];
					$oic['sn'] = $oc['subnet'];
				}
				
				if ($ipv6)
					$oic['sa'] = gen_subnet6($oic['ip'], $oic['sn']);
				else
					$oic['sa'] = gen_subnet($oic['ip'], $oic['sn']);
				$oic['ipv6ra'] = $oc['ipv6ra'];
			}
			
			$optcfg['opt' . $i] = $oic;
			$ifgroups['opt' . $i] = ($i * 100) + 200;
		}
	}
	
	if ($pptpdcfg['mode'] == "server" && !$ipv6) {
		$pptpip = $pptpdcfg['localip'];
		$pptpsa = $pptpdcfg['remoteip'];
		$pptpsn = subnet_size_to_cidr($pptpdcfg['nunits'] ? $pptpdcfg['nunits'] : 16);
	}
	
	if ($ipv6) {
		/* Make sure that group numbers are unique */
		foreach ($ifgroups as $key => $val) {
			$ifgroups[$key] += 10000;
		}
	}
	
	/* default block logging? */
	if (!isset($config['syslog']['nologdefaultblock']))
		$log = "log";
	else
		$log = "";
	
	$ipfrules = <<<EOD
# loopback
pass in quick on lo0 all
pass out quick on lo0 all

EOD;

	if (!$ipv6) {
		$ipfrules .= <<<EOD
# block short packets
block in $log quick all with short

# block IP options
block in $log quick all with ipopts

# allow access to DHCP server on LAN
pass in quick on $lanif proto udp from any port = 68 to 255.255.255.255 port = 67
pass in quick on $lanif proto udp from any port = 68 to $lanip port = 67
pass out quick on $lanif proto udp from $lanip port = 67 to any port = 68

EOD;

		/* allow access to DHCP server on optional interfaces */
		foreach ($optcfg as $on => $oc) {
			if ((isset($config['dhcpd'][$on]['enable']) && !$oc['bridge']) ||
					($oc['bridge'] && isset($config['dhcpd'][$oc['bridge_if']]['enable']))) {

				$ipfrules .= <<<EOD

# allow access to DHCP server on {$on}
pass in quick on {$oc['if']} proto udp from any port = 68 to 255.255.255.255 port = 67
pass in quick on {$oc['if']} proto udp from any port = 68 to {$oc['ip']} port = 67
pass out quick on {$oc['if']} proto udp from {$oc['ip']} port = 67 to any port = 68

EOD;
			}
		}
	}
	
	if ($ipv6) {
		/* Allow ICMPv6 NS/NA/toobig */
		$ipfrules .= <<<EOD

# allow essential ICMPv6 messages: router and neighbor soliciation/advertisement
pass in quick proto ipv6-icmp from any to any icmp-type 133
pass in quick proto ipv6-icmp from any to any icmp-type 134
pass in quick proto ipv6-icmp from any to any icmp-type 135
pass in quick proto ipv6-icmp from any to any icmp-type 136
pass out quick proto ipv6-icmp from any to any icmp-type 133
pass out quick proto ipv6-icmp from any to any icmp-type 134
pass out quick proto ipv6-icmp from any to any icmp-type 135
pass out quick proto ipv6-icmp from any to any icmp-type 136

# also allow ICMPv6 destination unreachable, packet too big (PMTUD) and time exceeded
pass in quick proto ipv6-icmp from any to any icmp-type 1
pass out quick proto ipv6-icmp from any to any icmp-type 1
pass in quick proto ipv6-icmp from any to any icmp-type 2
pass out quick proto ipv6-icmp from any to any icmp-type 2
pass in quick proto ipv6-icmp from any to any icmp-type 3
pass out quick proto ipv6-icmp from any to any icmp-type 3

EOD;
	} else {
		/* Need to pass IP protocol 41 if using 6to4 */
		if ($wancfg['ipaddr6'] == "6to4" && $curwanip) {
			$ipfrules .= <<<EOD

# allow 6to4 on WAN
pass in quick on $wanif proto ipv6 from any to $curwanip
pass out quick on $wanif proto ipv6 from $curwanip to any

EOD;
		}
	}
	
	/* pass traffic between statically routed subnets and the subnet on the
	   interface in question to avoid problems with complicated routing
	   topologies */
	$strtconf = 'route' . ($ipv6 ? '6' : '');
	if (isset($config['filter']['bypassstaticroutes']) && is_array($config['staticroutes'][$strtconf]) && count($config['staticroutes'][$strtconf])) {
		foreach ($config['staticroutes'][$strtconf] as $route) {
			unset($sa);
			
			if ($route['interface'] == "lan") {
				$ip = $lanip;
				$sa = $lansa;
				$sn = $lansn;
				$if = $lanif;
			} else if (strstr($route['interface'], "opt")) {
				$oc = $optcfg[$route['interface']];
				if ($oc['ip']) {
					$ip = $oc['ip'];
					$sa = $oc['sa'];
					$sn = $oc['sn'];
					$if = $oc['if'];	
				}
			}
			
			if ($sa) {
				$ipfrules .= <<<EOD
skip 2 in on {$if} from any to {$ip}
pass in quick on {$if} from {$sa}/{$sn} to {$route['network']}
pass in quick on {$if} from {$route['network']} to {$sa}/{$sn}
pass out quick on {$if} from {$sa}/{$sn} to {$route['network']}
pass out quick on {$if} from {$route['network']} to {$sa}/{$sn}

EOD;
			}
		}
	}

	$ipfrules .= <<<EOD

# WAN spoof check
block in $log quick on $wanif from $lansa/$lansn to any

EOD;

	foreach ($optcfg as $oc) {
		if (!$oc['bridge'])
			$ipfrules .= "block in $log quick on $wanif from {$oc['sa']}/{$oc['sn']} to any\n";
	}
	
	/* allow PPTP traffic if PPTP client is enabled on WAN */
	if ($wancfg['ipaddr'] == "pptp" && !$ipv6) {
		$ipfrules .= <<<EOD

# allow PPTP client
pass in quick on {$wancfg['if']} proto gre from any to any
pass out quick on {$wancfg['if']} proto gre from any to any
pass in quick on {$wancfg['if']} proto tcp from any port = 1723 to any
pass out quick on {$wancfg['if']} proto tcp from any to any port = 1723

EOD;
	}

    /* allow SIP signaling and RTP traffic if enabled. Need assigned public IP address  */
	if (isset($config['sip']['enable']) && $curwanip && !$ipv6) {
	$ipfrules .= <<<EOD

# allow SIP signaling and RTP traffic
pass in quick on {$wancfg['if']} proto udp from any to $curwanip port = {$config['sip']['port']}
pass in quick on {$wancfg['if']} proto udp from any to $curwanip port {$config['sip']['rtplow']} >< {$config['sip']['rtphigh']}

EOD;
    }

	if (!$ipv6) {
		$ipfrules .= <<<EOD

# allow our DHCP client out to the WAN
# XXX - should be more restrictive
# (not possible at the moment - need 'me' like in ipfw)
pass out quick on $wanif proto udp from any port = 68 to any port = 67
block in $log quick on $wanif proto udp from any port = 67 to $lansa/$lansn port = 68
pass in quick on $wanif proto udp from any port = 67 to any port = 68

# LAN/OPT spoof check (needs to be after DHCP because of broadcast addresses)

EOD;
	}

	/* LAN spoof check */
	/* omit if any interface is bridged to LAN and the filtering bridge is on */
	$lanbridge = false;
	foreach ($optcfg as $on => $oc) {
		if ($oc['bridge'] && $oc['bridge_if'] == "lan") {
			$lanbridge = true;
			break;
		}
	}
	if (!$lanbridge || !isset($config['bridge']['filteringbridge']))
		$ipfrules .= filter_rules_spoofcheck_generate('lan', $lanif, $lansa, $lansn, $log, $ipv6);

	/* OPT spoof check */
	foreach ($optcfg as $on => $oc) {
		/* omit for bridged interfaces when the filtering bridge is on */
		$isbridged = false;
		foreach ($optcfg as $on2 => $oc2) {
			if ($oc2['bridge'] && $oc2['bridge_if'] == $on) {
				$isbridged = true;
				break;
			}
		}
		
		if ($oc['ip'] && !(($oc['bridge'] || $isbridged) && isset($config['bridge']['filteringbridge'])))
			$ipfrules .= filter_rules_spoofcheck_generate($on, $oc['if'], $oc['sa'], $oc['sn'], $log, $ipv6);
	}
	
	/* block private networks on WAN? */
	if (isset($config['interfaces']['wan']['blockpriv'])) {
		if (!$ipv6) {
			$ipfrules .= <<<EOD

# block anything from private networks on WAN interface
block in $log quick on $wanif from 10.0.0.0/8 to any
block in $log quick on $wanif from 127.0.0.0/8 to any
block in $log quick on $wanif from 172.16.0.0/12 to any
block in $log quick on $wanif from 192.168.0.0/16 to any

EOD;
		} else {
			/* source: /etc/rc.firewall6 */
			$ipfrules .= <<<EOD

# Stop site-local on the outside interface
block in $log quick on $wanif from fec0::/10 to any
block in $log quick on $wanif from any to fec0::/10

# Disallow "internal" addresses to appear on the wire.
block in $log quick on $wanif from ::ffff:0.0.0.0/96 to any
block in $log quick on $wanif from any to ::ffff:0.0.0.0/96

# Disallow packets to malicious IPv4 compatible prefix.
block in $log quick on $wanif from ::224.0.0.0/100 to any
block in $log quick on $wanif from any to ::224.0.0.0/100
block in $log quick on $wanif from ::127.0.0.0/104 to any
block in $log quick on $wanif from any to ::127.0.0.0/104
block in $log quick on $wanif from ::0.0.0.0/104 to any
block in $log quick on $wanif from any to ::0.0.0.0/104
block in $log quick on $wanif from ::255.0.0.0/104 to any
block in $log quick on $wanif from any to ::255.0.0.0/104

block in $log quick on $wanif from ::0.0.0.0/96 to any
block in $log quick on $wanif from any to ::0.0.0.0/96

# Disallow packets to malicious 6to4 prefix.
block in $log quick on $wanif from 2002:e000::/20 to any
block in $log quick on $wanif from any to 2002:e000::/20
block in $log quick on $wanif from 2002:7f00::/24 to any
block in $log quick on $wanif from any to 2002:7f00::/24
block in $log quick on $wanif from 2002:0000::/24 to any
block in $log quick on $wanif from any to 2002:0000::/24
block in $log quick on $wanif from 2002:ff00::/24 to any
block in $log quick on $wanif from any to 2002:ff00::/24
block in $log quick on $wanif from 2002:0a00::/24 to any
block in $log quick on $wanif from any to 2002:0a00::/24
block in $log quick on $wanif from 2002:ac10::/28 to any
block in $log quick on $wanif from any to 2002:ac10::/28
block in $log quick on $wanif from 2002:c0a8::/32 to any
block in $log quick on $wanif from any to 2002:c0a8::/32

block in $log quick on $wanif from ff05::/16 to any
block in $log quick on $wanif from any to ff05::/16

EOD;
		}
	}
	
	/* IPsec enabled? */
	if (!$ipv6 && isset($config['ipsec']['enable']) && 
		((is_array($config['ipsec']['tunnel']) &&
		count($config['ipsec']['tunnel'])) ||
			isset($config['ipsec']['mobileclients']['enable']))) {
		
		if ($curwanip)
			$ipfrules .= filter_rules_ipsec_generate($wanif, $curwanip);
		
		$ipfrules .= filter_rules_ipsec_generate($lanif, $lanip);
		
		foreach ($optcfg as $on => $oc) {
			if ($oc['ip'])
				$ipfrules .= filter_rules_ipsec_generate($oc['if'], $oc['ip']);
		}
	}

	$out_kf = "";
	if (isset($config['filter']['allowipsecfrags']))
		$out_kf = "keep frags";

	/* XXX - the first section is only needed because ipf refuses to
		parse rules that have "flags S/SAFR" and proto "tcp/udp" set because
		UDP does not have flags, but we still want to offer the TCP/UDP protocol
		option to the user */
		
	$ipfrules .= <<<EOD

# Block TCP packets that do not mark the start of a connection
skip 1 in proto tcp all flags S/SAFR
block in $log quick proto tcp all

#---------------------------------------------------------------------------
# group head {$ifgroups['lan']} - LAN interface
#---------------------------------------------------------------------------
block in $log quick on $lanif all head {$ifgroups['lan']}

# let out anything from the firewall host itself and decrypted IPsec traffic
pass out quick on $lanif all keep state $out_kf

#---------------------------------------------------------------------------
# group head {$ifgroups['wan']} - WAN interface
#---------------------------------------------------------------------------
block in $log quick on $wanif all head {$ifgroups['wan']}

# let out anything from the firewall host itself and decrypted IPsec traffic
pass out quick on $wanif all keep state $out_kf

EOD;

	/* group heads for optional interfaces */
	foreach ($optcfg as $on => $oc) {
	
		$ingroup = $ifgroups[$on];
	
		$ipfrules .= <<<EOD
		
#---------------------------------------------------------------------------
# group head {$ingroup} - {$on} interface
#---------------------------------------------------------------------------
block in $log quick on {$oc['if']} all head {$ingroup}

# let out anything from the firewall host itself and decrypted IPsec traffic
pass out quick on {$oc['if']} all keep state $out_kf

EOD;
	
	}
	
	if (!$ipv6 && isset($config['ipsec']['enable'])) {
		$ipfrules .= <<<EOD

# always pass outgoing IPsec encapsulated packets
pass out quick on enc0 all keep state $out_kf

EOD;
	}
	
	if (!isset($config['system']['webgui']['noantilockout'])) {
		$ipfrules .= <<<EOD

# make sure the user cannot lock himself out of the webGUI
pass in quick from $lansa/$lansn to $lanip keep state group {$ifgroups['lan']}

EOD;
	}
	
	/* PPTPd enabled? */
	if (!$ipv6 && $pptpdcfg['mode'] && ($pptpdcfg['mode'] != "off") && !isset($pptpdcfg['nofwrulegen'])) {
	
		if ($pptpdcfg['mode'] == "server")
			$pptpdtarget = $curwanip;
		else
			$pptpdtarget = $pptpdcfg['redir'];
		
		if ($pptpdtarget) {
			$ipfrules .= <<<EOD

# PPTP rules
pass in quick proto gre from any to $pptpdtarget keep state group 200
pass in quick proto tcp from any to $pptpdtarget port = 1723 keep state group 200

EOD;
		}
	}
	
	$ipfrules .= "\n# User-defined rules follow\n";
	
	$i = -1;
	
	if (isset($config['filter'][$ipv6 ? 'rule6' : 'rule']))
		foreach ($config['filter'][$ipv6 ? 'rule6' : 'rule'] as $rule) {
		
		$i++;
		
		/* don't include disabled rules */
		if (isset($rule['disabled']))
			continue;
		
		/* does the rule deal with a PPTP or IPsec interface? */
		if ($rule['interface'] == "pptp") {
		
			if ($pptpdcfg['mode'] != "server")
				continue;
				
			$nif = $pptpdcfg['nunits'];
			if (!$nif)
				$nif = 16;
			$ispptp = true;
			$isipsec = false;
		} else if ($rule['interface'] == "ipsec") {
		
			if (!isset($config['ipsec']['enable']))
				continue;
		
			$nif = 1;
			$ispptp = false;
			$isipsec = true;
		} else {
			
			if (strstr($rule['interface'], "opt")) {
				if (!array_key_exists($rule['interface'], $optcfg))
					continue;
			}
			
			$nif = 1;
			$ispptp = false;
			$isipsec = false;
		}
		
		if ($pptpdcfg['mode'] != "server") {
			if (($rule['source']['network'] == "pptp") ||
				($rule['destination']['network'] == "pptp"))
					continue;
		}
		
		if ($rule['source']['network'] && strstr($rule['source']['network'], "opt")) {
			if (!array_key_exists($rule['source']['network'], $optcfg))
				continue;
		}
		if ($rule['destination']['network'] && strstr($rule['destination']['network'], "opt")) {
			if (!array_key_exists($rule['destination']['network'], $optcfg))
				continue;
		}
		
		/* check for unresolvable aliases */
		if ($rule['source']['address'] && !alias_expand($rule['source']['address']))
			continue;
		if ($rule['destination']['address'] && !alias_expand($rule['destination']['address']))
			continue;
		
		for ($iif = 0; $iif < $nif; $iif++) {
			
			if (!$ispptp && !$isipsec) {
			
				$groupnum = $ifgroups[$rule['interface']];
				
				if (!$groupnum) {
					printf("Invalid interface name in rule $i\n");
					break;
				}
			}
			
			$type = $rule['type'];
			if ($type != "pass" && $type != "block" && $type != "reject") {
				/* default (for older rules) is pass */
				$type = "pass";
			}

			if ($type == "reject") {
				/* special reject packet */
				if ($rule['protocol'] == "tcp") {
					$line = "block return-rst";
				} else if ($rule['protocol'] == "udp") {
					$line = "block return-icmp(port-unr)";
				} else {
					$line = "block";
				}
			} else {
				$line = $type;
			}
			
			$line .= " in ";

			if (isset($rule['log']))
				$line .= "log first ";

			$line .= "quick ";
			
			if ($ispptp)
				$line .= "on ng" . ($iif+1) . " ";
			else if ($isipsec)
				$line .= "on enc0 ";
			
			if (isset($rule['protocol'])) {
				$line .= "proto {$rule['protocol']} ";
			}
			
			/* source address */
			if (isset($rule['source']['any'])) {
				$src = "any";
			} else if ($rule['source']['network']) {
				
				if (strstr($rule['source']['network'], "opt")) {
					$src = $optcfg[$rule['source']['network']]['sa'] . "/" .
						$optcfg[$rule['source']['network']]['sn'];
				} else {
					switch ($rule['source']['network']) {
						case 'wanip':
							$src = $curwanip;
							break;
						case 'lan':
							$src = "$lansa/$lansn";
							break;
						case 'pptp':
							$src = "$pptpsa/$pptpsn";
							break;
					}
				}
			} else if ($rule['source']['address']) {
				$src = alias_expand($rule['source']['address']);
			}
			
			if (!$src || ($src == "/")) {
				//printf("No source address found in rule $i\n");
				break;
			}
			
			if (isset($rule['source']['not'])) {
				$line .= "from !$src ";
			} else {
				$line .= "from $src ";
			}
			
			if (in_array($rule['protocol'], array("tcp","udp","tcp/udp"))) {
				
				if ($rule['source']['port']) {
					$srcport = explode("-", $rule['source']['port']);
					
					if ((!$srcport[1]) || ($srcport[0] == $srcport[1])) {
						$line .= "port = {$srcport[0]} ";
					} else if (($srcport[0] == 1) && ($srcport[1] == 65535)) {
						/* no need for a port statement here */
					} else if ($srcport[1] == 65535) {
						$line .= "port >= {$srcport[0]} "; 
					} else if ($srcport[0] == 1) {
						$line .= "port <= {$srcport[1]} "; 
					} else {
						$srcport[0]--;
						$srcport[1]++;
						$line .= "port {$srcport[0]} >< {$srcport[1]} ";
					}
				}
			}
			
			/* destination address */
			if (isset($rule['destination']['any'])) {
				$dst = "any";
			} else if ($rule['destination']['network']) {
				
				if (strstr($rule['destination']['network'], "opt")) {
					$dst = $optcfg[$rule['destination']['network']]['sa'] . "/" .
						$optcfg[$rule['destination']['network']]['sn'];
				} else {
					switch ($rule['destination']['network']) {
						case 'wanip':
							$dst = $curwanip;
							break;
						case 'lan':
							$dst = "$lansa/$lansn";
							break;
						case 'pptp':
							$dst = "$pptpsa/$pptpsn";
							break;
					}
				}
			} else if ($rule['destination']['address']) {
				$dst = alias_expand($rule['destination']['address']);
			}
			
			if (!$dst || ($dst == "/")) {
				//printf("No destination address found in rule $i\n");
				break;
			}
			
			if (isset($rule['destination']['not'])) {
				$line .= "to !$dst ";
			} else {
				$line .= "to $dst ";
			}
			
			if (in_array($rule['protocol'], array("tcp","udp","tcp/udp"))) {
				
				if ($rule['destination']['port']) {
					$dstport = explode("-", $rule['destination']['port']);
					
					if ((!$dstport[1]) || ($dstport[0] == $dstport[1])) {
						$line .= "port = {$dstport[0]} ";
					} else if (($dstport[0] == 1) && ($dstport[1] == 65535)) {
						/* no need for a port statement here */
					} else if ($dstport[1] == 65535) {
						$line .= "port >= {$dstport[0]} "; 
					} else if ($dstport[0] == 1) {
						$line .= "port <= {$dstport[1]} "; 
					}  else {
						$dstport[0]--;
						$dstport[1]++;
						$line .= "port {$dstport[0]} >< {$dstport[1]} ";
					}
				}
			}
			
			if (($rule['protocol'] == "icmp") && $rule['icmptype']) {
				$line .= "icmp-type {$rule['icmptype']} ";
			}
			
			if ($type == "pass") {
				$line .= "keep state ";
			
				if (isset($rule['frags']))
					$line .= "keep frags ";
			}
			
			if ($type == "reject" && $rule['protocol'] == "tcp") {
				/* special reject packet */
				$line .= "flags S/SA ";
			}
				
			if (!$ispptp && !$isipsec) {
				$line .= "group $groupnum ";
			}
			
			$line .= "\n";
			
			$ipfrules .= $line;
		}
	}
	
	$ipfrules .= <<<EOD
	
#---------------------------------------------------------------------------
# default rules (just to be sure)
#---------------------------------------------------------------------------
block in $log quick all
block out $log quick all

EOD;
	
	return $ipfrules;
}

function filter_rules_spoofcheck_generate($ifname, $if, $sa, $sn, $log, $ipv6 = false) {

	global $g, $config;
	
	$ipfrules = "";

	$strtconf = 'route' . ($ipv6 ? '6' : '');
	if (is_array($config['staticroutes'][$strtconf]) && count($config['staticroutes'][$strtconf])) {
		/* count rules */
		$n = 1;
		foreach ($config['staticroutes'][$strtconf] as $route) {
			if ($route['interface'] == $ifname)
				$n++;
		}
		
		/* output skip rules */
		foreach ($config['staticroutes'][$strtconf] as $route) {
			if ($route['interface'] == $ifname) {
				$ipfrules .= "skip $n in on $if from {$route['network']} to any\n";
				$n--;
			}
		}
		$ipfrules .= "skip 1 in on $if from $sa/$sn to any\n";
		$ipfrules .= "block in $log quick on $if all\n";
	} else {
		$ipfrules .= "block in $log quick on $if from ! $sa/$sn to any\n";
	}
	
	return $ipfrules;
}

function filter_rules_ipsec_generate($ifname, $ip) {

	$ipfrules = <<<EOD

# Pass IKE packets
pass in quick on {$ifname} proto udp from any to {$ip} port = 500 keep frags
pass out quick on {$ifname} proto udp from {$ip} port = 500 to any keep frags

# Pass NAT-T encapsulated ESP packets
pass in quick on {$ifname} proto udp from any to {$ip} port = 4500 keep frags
pass out quick on {$ifname} proto udp from {$ip} port = 4500 to any keep frags

# Pass ESP packets
pass in quick on {$ifname} proto esp from any to {$ip} keep frags
pass out quick on {$ifname} proto esp from {$ip} to any keep frags

# Pass AH packets
pass in quick on {$ifname} proto ah from any to {$ip} keep frags
pass out quick on {$ifname} proto ah from {$ip} to any keep frags

EOD;
	
	return $ipfrules;
}

?>
