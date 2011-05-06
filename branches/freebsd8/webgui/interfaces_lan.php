#!/usr/local/bin/php
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

$pgtitle = array("Interfaces", "LAN");
require("guiconfig.inc");

$wancfg = &$config['interfaces']['wan'];
$lancfg = &$config['interfaces']['lan'];
$optcfg = &$config['interfaces']['lan'];
$pconfig['ipaddr'] = $config['interfaces']['lan']['ipaddr'];
$pconfig['subnet'] = $config['interfaces']['lan']['subnet'];
if (ipv6enabled()) {
	$pconfig['ipv6ra'] = isset($config['interfaces']['lan']['ipv6ra']);	
	$pconfig['ipv6ramtu'] = $config['interfaces']['lan']['ipv6ramtu'];
	
	if (isset($config['interfaces']['lan']['ipv6rao'])) {
		$pconfig['raflags'] = "Other";
	} else if (isset($config['interfaces']['lan']['ipv6ram'])) {
		$pconfig['raflags'] = "Managed";
	} else {
		$pconfig['raflags'] = "None";
	}
	
	if ($config['interfaces']['lan']['ipaddr6'] == "6to4") {
		$pconfig['ipv6mode'] = "6to4";
	} else if ($config['interfaces']['lan']['ipaddr6'] == "DHCP-PD") {
		$pconfig['ipv6mode'] = "DHCP-PD";
		$pconfig['slalen'] = $config['interfaces']['lan']['slalen'];
		$pconfig['slaid'] = $config['interfaces']['lan']['slaid'];
	} else if ($config['interfaces']['lan']['ipaddr6']) {
		$pconfig['ipaddr6'] = $config['interfaces']['lan']['ipaddr6'];
		$pconfig['subnet6'] = $config['interfaces']['lan']['subnet6'];
		$pconfig['ipv6mode'] = "static";
	} else {
		$pconfig['ipv6mode'] = "disabled";
	}
}

if ($_POST) {

	unset($input_errors);
	$pconfig = $_POST;

	/* input validation */
	$reqdfields = explode(" ", "ipaddr subnet");
	$reqdfieldsn = explode(",", "IP address,Subnet bit count");
	
	do_input_validation($_POST, $reqdfields, $reqdfieldsn, &$input_errors);
	
	if (($_POST['ipaddr'] && !is_ipaddr($_POST['ipaddr']))) {
		$input_errors[] = "A valid IP address must be specified.";
	}
	if (($_POST['subnet'] && !is_numeric($_POST['subnet']))) {
		$input_errors[] = "A valid subnet bit count must be specified.";
	}
	
	if (ipv6enabled()) {
		if ($_POST['ipv6mode'] == "static" && !is_ipaddr6($_POST['ipaddr6'])) {
			$input_errors[] = "A valid IPv6 address must be specified.";
		}
		if ($_POST['ipv6ramtu'] && ($_POST['ipv6ramtu'] < 56 || $_POST['ipv6ramtu'] > 1500)) {
			$input_errors[] = "A valid RA MTU must be specified (56 - 1500).";
		}
	}

	if (!$input_errors) {
		$v4changed = ($_POST['ipaddr'] != $config['interfaces']['lan']['ipaddr'] || $_POST['subnet'] != $config['interfaces']['lan']['subnet']);
		$config['interfaces']['lan']['ipaddr'] = $_POST['ipaddr'];
		$config['interfaces']['lan']['subnet'] = $_POST['subnet'];
		
		if (ipv6enabled()) {
			$oldipaddr6 = $config['interfaces']['lan']['ipaddr6'];
			$oldsubnet6 = $config['interfaces']['lan']['subnet6'];
			$oldipv6ra = $config['interfaces']['lan']['ipv6ra'];
			$oldipv6ramtu = $config['interfaces']['lan']['ipv6ramtu'];
			$oldipv6slalen = $config['interfaces']['lan']['slalen'];
			$oldipv6slaid = $config['interfaces']['lan']['slaid'];
			
			if ($_POST['raflags'] == "Managed") {
				unset($config['interfaces']['lan']['ipv6rao']);
				$config['interfaces']['lan']['ipv6ram'] = true;
			} else if ($_POST['raflags'] == "Other") {
				unset($config['interfaces']['lan']['ipv6ram']);
				$config['interfaces']['lan']['ipv6rao'] = true;
			} else if ($_POST['raflags'] == "None") {
				unset($config['interfaces']['lan']['ipv6rao']);
				unset($config['interfaces']['lan']['ipv6ram']);
			}
			
			if ($_POST['ipv6mode'] == "6to4") {
				$config['interfaces']['lan']['ipaddr6'] = "6to4";
				unset($config['interfaces']['lan']['subnet6']);
				$config['interfaces']['lan']['ipv6ra'] = $_POST['ipv6ra'] ? true : false;
				$config['interfaces']['lan']['ipv6ramtu'] = $_POST['ipv6ramtu'] ;
			} else if ($_POST['ipv6mode'] == "DHCP-PD") {
				$config['interfaces']['lan']['ipaddr6'] = "DHCP-PD";
				unset($config['interfaces']['lan']['subnet6']);
				unset($config['interfaces']['lan']['slaid']);
				if ($_POST['slaid'] != null) 
					$config['interfaces']['lan']['slaid'] = $_POST['slaid'];
				$config['interfaces']['lan']['slalen'] = 64 - $_POST['ispfix'];
				$pconfig['slalen'] = 64 - $_POST['ispfix'];
				$config['interfaces']['lan']['ipv6ra'] = $_POST['ipv6ra'] ? true : false;
				$config['interfaces']['lan']['ipv6ramtu'] = $_POST['ipv6ramtu'] ;
			} else if ($_POST['ipv6mode'] == "static") {
				$config['interfaces']['lan']['ipaddr6'] = $_POST['ipaddr6'];
				$config['interfaces']['lan']['subnet6'] = $_POST['subnet6'];
				$config['interfaces']['lan']['ipv6ra'] = $_POST['ipv6ra'] ? true : false;
				$config['interfaces']['lan']['ipv6ramtu'] = $_POST['ipv6ramtu'] ;
			} else {
				unset($config['interfaces']['lan']['ipaddr6']);
				unset($config['interfaces']['lan']['subnet6']);
				unset($config['interfaces']['lan']['ipv6ra']);
				unset($config['interfaces']['lan']['ipv6ramtu']);
			}
			
			$v6changed = ($oldipaddr6 != $config['interfaces']['lan']['ipaddr6'] ||
						  $oldsubnet6 != $config['interfaces']['lan']['subnet6'] ||
						  $oldipv6slalen != $config['interfaces']['lan']['slalen'] ||
						  $oldipv6slaid != $config['interfaces']['lan']['slanid'] ||
						  $pconfig['raflags'] != $_POST['raflags'] ||
						  isset($oldipv6ra) != isset($_POST['ipv6ra']) ||
						  isset($oldipv6ram) != isset($newipv6ram) ||
						  $oldipv6ramtu != $_POST['ipv6ramtu']);
		}
		
		$dhcpd_disabled = false;
		if ($v4changed && isset($config['dhcpd']['lan']['enable'])) {
			unset($config['dhcpd']['lan']['enable']);
			$dhcpd_disabled = true;
		}
		
		write_config();
		
		if ($v4changed)
			touch($d_sysrebootreqd_path);
		else if ($v6changed)
			interfaces_lan_configure6();
		
		interfaces_secondaries_configure('lan');
		
		$savemsg = get_std_save_message(0);
		
		if ($dhcpd_disabled)
			$savemsg .= "<br><strong>Note that the DHCP server has been disabled.<br>Please review its configuration " .
				"and enable it again prior to rebooting.</strong>";
	}
}
?>
<?php include("fbegin.inc"); ?>
<script type="text/javascript">
<!--
function enable_change(enable_over) {
<?php if (ipv6enabled()): ?>
	var en = (document.iform.ipv6mode.selectedIndex == 1 || enable_over);
	var pd = (document.iform.ipv6mode.selectedIndex == 3 || enable_over);
	document.iform.ipaddr6.disabled = !en;
	document.iform.subnet6.disabled = !en;
	document.iform.ipv6ra.disabled = !(document.iform.ipv6mode.selectedIndex != 0 || enable_over);
	document.iform.ipv6ramtu.disabled = !(document.iform.ipv6mode.selectedIndex != 0 || enable_over);
	document.iform.raflags.disabled = !(document.iform.ipv6mode.selectedIndex != 0 || enable_over);
	document.iform.ispfix.disabled = !pd;
	document.iform.slaid.disabled = !pd;
<?php endif; ?>
}
// -->
</script>
<?php if ($input_errors) print_input_errors($input_errors); ?>
<?php if ($savemsg) print_info_box($savemsg); ?>

<table width="100%" border="0" cellpadding="0" cellspacing="0" summary="tab pane">
  <tr><td class="tabnavtbl">
  <ul id="tabnav">
    <li class="tabact">Primary configuration</li>
	<li class="tabinact1"><a href="interfaces_secondaries.php?if=lan&amp;ifname=LAN">Secondary IPs</a></li>
  </ul>
  </td></tr>
  <tr> 
    <td class="tabcont">
<table width="100%" border="0" cellpadding="0" cellspacing="0" summary="content pane">
            <form action="interfaces_lan.php" method="post" name="iform" id="iform">
              <table width="100%" border="0" cellpadding="6" cellspacing="0" summary="content pane">
                <tr> 
                  <td width="22%" valign="top" class="vncellreq">IP address</td>
                  <td width="78%" class="vtable"> 
                    <?=$mandfldhtml;?><input name="ipaddr" type="text" class="formfld" id="ipaddr" size="20" value="<?=htmlspecialchars($pconfig['ipaddr']);?>">
                    / 
                    <select name="subnet" class="formfld" id="subnet">
                      <?php for ($i = 31; $i > 0; $i--): ?>
                      <option value="<?=$i;?>" <?php if ($i == $pconfig['subnet']) echo "selected"; ?>>
                      <?=$i;?>
                      </option>
                      <?php endfor; ?>
                    </select></td>
                </tr>
                <?php if (ipv6enabled()): ?>
                <tr> 
                  <td valign="top" class="vncellreq">IPv6 mode</td>
                  <td class="vtable"> 
                    <select name="ipv6mode" class="formfld" id="ipv6mode" onchange="enable_change(false)">
                      <?php $opts = array('disabled', 'static', '6to4','DHCP-PD');
						foreach ($opts as $opt) {
							echo "<option value=\"$opt\"";
							if ($opt == $pconfig['ipv6mode']) echo "selected";
							echo ">$opt</option>\n";
						}
						?>
                    </select><br>
					Choosing 6to4 on the LAN interface will make it use the first available /64 prefix within
					the WAN interface&apos;s 6to4 prefix (which is determined by its current IPv4 address).</td>
                </tr>
                <tr> 
                  <td valign="top" class="vncellreq">IPv6 address</td>
                  <td class="vtable"> 
                    <input name="ipaddr6" type="text" class="formfld" id="ipaddr6" size="30" value="<?=htmlspecialchars($pconfig['ipaddr6']);?>">
                    / 
                    <select name="subnet6" class="formfld" id="subnet6">
                      <?php for ($i = 127; $i > 0; --$i): ?>
                      <option value="<?=$i;?>" <?php
                        if ($i == $pconfig['subnet6'] || (!isset($pconfig['subnet6']) && $i == 64)) echo "selected";
                      ?>>
                      <?=$i;?>
                      </option>
                      <?php endfor; ?>
                    </select><br>
					   Using a number less than /64 will cause RAs not to be announced as there aren't enough subnet bits. i.e. /48</td>
                </tr>
                 <tr> 
                  <td valign="top" class="vncellreq">IPv6 Prefix Delegation</td>
                  <td class="vtable"> 
                    <input name="slaid" type="text" class="formfld" id="slaid" size="4" value="<?=htmlspecialchars($pconfig['slaid']);?>">
                    / 
                    <select name="ispfix" class="formfld" id="ispfix">
                      <?php for ($l = 64; $l > 0; --$l): ?>
                      <option value="<?=$l;?>" <?php
                        if ($l == 64 - $pconfig['slalen'] || (!isset($pconfig['slalen']) && $l == 64)) echo "selected";
                      ?>>
                      <?=$l;?>
                      </option>
                      <?php endfor; ?>
                    </select><br>
					   Select site-level aggregator ID and ISP prefix length (decimal notation).<br>
					   If ID is 10 and the client is delegated an IPv6 prefix 2001:db8:ffff and prefix 64, this will result in
					   a single IPv6 prefix, 2001:db8:ffff:A::/64 .</td>
                </tr>
                <tr> 
                  <td valign="top" class="vncellreq">IPv6 RA</td>
                  <td class="vtable"> 
					<input type="checkbox" name="ipv6ra" id="ipv6ra" onchange="enable_change(false)" value="1" <?php if ($pconfig['ipv6ra']) echo "checked";?> > <strong>Send IPv6 router advertisements</strong><br>
					If this option is checked, other hosts on this interface will be able to automatically configure
					their IPv6 address based on prefix and gateway information that the firewall provides to them.
					This option should normally be enabled. <?php echo $pconfig['raflags']?> <br>
                    <strong>Flags</strong> <select name="raflags" class="formfld" id="raflags">
                      <?php $opts = array('None', 'Managed', 'Other');
						foreach ($opts as $opt) {
							echo "<option value=\"$opt\"";
							if ($opt == $pconfig['raflags']) echo " selected";
							echo ">$opt</option>\n";
						}
						?>
                    </select><br>
					If Managed is selected, other hosts on this interface will use DHCPv6 for BOTH address and non address allocation configuration.<br>
					If Other is selected, other hosts on this interface will use DHCPv6 for non address allocation only, like DNS.<br>
                    <strong>MTU</strong> <input name="ipv6ramtu" type="text" class="formfld" id="ipv6ramtu" size="4" value="<?=htmlspecialchars($pconfig['ipv6ramtu']);?>"> bytes <br>
                    Enter the MTU you want advertised here.<br>
				</td>
                </tr>
                <?php endif; ?>
                <tr> 
                  <td width="22%" valign="top">&nbsp;</td>
                  <td width="78%"> 
                    <input name="Submit" type="submit" class="formbtn" value="Save" onclick="enable_change(true)"> 
                  </td>
                </tr>
                <tr> 
                  <td width="22%" valign="top">&nbsp;</td>
                  <td width="78%">
                    <span class="vexpl">
                      <span class="red">
                        <strong>Warning:</strong><br>
                      </span>after you click &quot;Save&quot;, you must 
                      reboot your firewall for changes to take effect. You may also 
                      have to do one or more of the following steps before you can 
                      access your firewall again:
                    </span> 
                    <ul>
                      <li>change the IP address of your computer</li>
                      <li>renew its DHCP lease</li>
                      <li>access the webGUI with the new IP address</li>
                    </ul>
                  </td>
                </tr>
              </table>
</form>
<script language="JavaScript">
<!--
enable_change(false);
//-->
</script>
</table>
<?php include("fend.inc"); ?>
