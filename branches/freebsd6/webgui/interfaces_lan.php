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
	$pconfig['ipv6ram'] = isset($config['interfaces']['lan']['ipv6ram']);
	$pconfig['ipv6rao'] = isset($config['interfaces']['lan']['ipv6rao']);
	
	if ($config['interfaces']['lan']['ipaddr6'] == "6to4") {
		$pconfig['ipv6mode'] = "6to4";
	} else if ($config['interfaces']['lan']['ipaddr6']) {
		$pconfig['ipaddr6'] = $config['interfaces']['lan']['ipaddr6'];
		$pconfig['subnet6'] = $config['interfaces']['lan']['subnet6'];
		$pconfig['ipv6mode'] = "static";
	} else {
		$pconfig['ipv6mode'] = "disabled";
	}
}

/* Wireless interface? */
if (isset($optcfg['wireless'])) {
	require("interfaces_wlan.inc");
	wireless_config_init();
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
	}
	
	/* Wireless interface? */
	if (isset($optcfg['wireless'])) {
		$wi_input_errors = wireless_config_post();
		if ($wi_input_errors) {
			$input_errors = array_merge($input_errors, $wi_input_errors);
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
			$oldipv6ram = $config['interfaces']['lan']['ipv6ram'];
			$oldipv6rao = $config['interfaces']['lan']['ipv6rao'];
			
			if ($_POST['ipv6mode'] == "6to4") {
				$config['interfaces']['lan']['ipaddr6'] = "6to4";
				unset($config['interfaces']['lan']['subnet6']);
				$config['interfaces']['lan']['ipv6ra'] = $_POST['ipv6ra'] ? true : false;
				$config['interfaces']['lan']['ipv6ram'] = $_POST['ipv6ram'] ? true : false;
				$config['interfaces']['lan']['ipv6rao'] = $_POST['ipv6rao'] ? true : false;
			} else if ($_POST['ipv6mode'] == "static") {
				$config['interfaces']['lan']['ipaddr6'] = $_POST['ipaddr6'];
				$config['interfaces']['lan']['subnet6'] = $_POST['subnet6'];
				$config['interfaces']['lan']['ipv6ra'] = $_POST['ipv6ra'] ? true : false;
				$config['interfaces']['lan']['ipv6ram'] = $_POST['ipv6ram'] ? true : false;
				$config['interfaces']['lan']['ipv6rao'] = $_POST['ipv6rao'] ? true : false;
			} else {
				unset($config['interfaces']['lan']['ipaddr6']);
				unset($config['interfaces']['lan']['subnet6']);
				unset($config['interfaces']['lan']['ipv6ra']);
				unset($config['interfaces']['lan']['ipv6ram']);
				unset($config['interfaces']['lan']['ipv6rao']);
			}
			
			$v6changed = ($oldipaddr6 != $config['interfaces']['lan']['ipaddr6'] ||
						  $oldsubnet6 != $config['interfaces']['lan']['subnet6'] ||
						  isset($oldipv6ra) != isset($_POST['ipv6ra'])  ||
						  isset($oldipv6ram) != isset($_POST['ipv6ram'])  ||
						  isset($oldipv6rao) != isset($_POST['ipv6rao']));
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
	document.iform.ipaddr6.disabled = !en;
	document.iform.subnet6.disabled = !en;
	document.iform.ipv6ra.disabled = !(document.iform.ipv6mode.selectedIndex != 0 || enable_over);
	document.iform.ipv6ram.disabled = !(document.iform.ipv6mode.selectedIndex != 0 || enable_over);
	document.iform.ipv6rao.disabled = !(document.iform.ipv6mode.selectedIndex != 0 || enable_over);
<?php endif; ?>
	
	if (document.iform.mode) {
		 wlan_enable_change(enable_over);
	}
}
// -->
</script>
<?php if (isset($optcfg['wireless'])): ?>
<script language="javascript" src="interfaces_wlan.js"></script>
<?php endif; ?>
<?php if ($input_errors) print_input_errors($input_errors); ?>
<?php if ($savemsg) print_info_box($savemsg); ?>
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
                      <?php $opts = array('disabled', 'static', '6to4');
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
                    </select></td>
                </tr>
				<tr> 
	                <td valign="top" class="vncellreq">Suggested IPv6 address</td>
	                <td class="vtable"> 
	                <?php if (!isset($wancfg['ipv6ra'])): ?>
					Router advertisements are not enabled on WAN interface!
	 				<?php else: ?>
					<strong><?php echo suggest_ipv6_lan_addr() ?></strong><br>
					This IPv6 address is suggested from listening to prefix advertisements recieved on the WAN interface, and using the first address available in that prefix.
					<?php endif; ?>
					</td>
	            </tr>
                <tr> 
                  <td valign="top" class="vncellreq">IPv6 RA</td>
                  <td class="vtable"> 
					<input type="checkbox" name="ipv6ra" id="ipv6ra" value="1" <?php if ($pconfig['ipv6ra']) echo "checked";?>> <strong>Send IPv6 router advertisements</strong><br>
					If this option is checked, other hosts on this interface will be able to automatically configure
					their IPv6 address based on prefix and gateway information that the firewall provides to them.
					This option should normally be enabled.<br>
					<input type="checkbox" name="ipv6ram" id="ipv6ram" value="1" <?php if ($pconfig['ipv6ram']) echo "checked";?>> <strong>Managed address configuration</strong><br>
					If this option is checked, other hosts on this interface will use DHCPv6 for address allocation and non address allocation configuration.<br>
					<input type="checkbox" name="ipv6rao" id="ipv6rao" value="1" <?php if ($pconfig['ipv6rao']) echo "checked";?>> <strong>Other stateful configuration</strong><br>
					If this option is checked, other hosts on this interface will use DHCPv6 for non address allocaiton configuration, such as DNS.
                  </td>
                </tr>
                <?php endif; ?>
				<?php /* Wireless interface? */
				if (isset($optcfg['wireless']))
					wireless_config_print();
				?>
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
<?php include("fend.inc"); ?>
