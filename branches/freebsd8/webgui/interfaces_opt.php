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

require("guiconfig.inc");

unset($index);
if ($_GET['index'])
	$index = $_GET['index'];
else if ($_POST['index'])
	$index = $_POST['index'];
	
if (!$index)
	exit;

$optcfg = &$config['interfaces']['opt' . $index];
$pconfig['descr'] = $optcfg['descr'];
$pconfig['bridge'] = $optcfg['bridge'];
$pconfig['ipaddr'] = $optcfg['ipaddr'];
$pconfig['subnet'] = $optcfg['subnet'];
$pconfig['enable'] = isset($optcfg['enable']);
if (ipv6enabled()) {
	$pconfig['ipv6ra'] = isset($optcfg['ipv6ra']);
	$pconfig['ipv6ramtu'] = isset($optcfg['ipv6ramtu']);
	
	if (isset($optcfg['ipv6rao'])) {
		$pconfig['raflags'] = "Other";
	} else if (isset($optcfg['ipv6ram'])) {
		$pconfig['raflags'] = "Managed";
	} else {
		$pconfig['raflags'] = "None";
	}
	
	if ($optcfg['ipaddr6'] == "6to4") {
		$pconfig['ipv6mode'] = "6to4";
	} else if ($optcfg['ipaddr6'] == "DHCP-PD") {
		$pconfig['ipv6mode'] = "DHCP-PD";
		$pconfig['slalen'] = $optcfg['slalen'];
		$pconfig['slaid'] = $optcfg['slaid'];
	} else if ($optcfg['ipaddr6']) {
		$pconfig['ipaddr6'] = $optcfg['ipaddr6'];
		$pconfig['subnet6'] = $optcfg['subnet6'];
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
	if ($_POST['enable']) {
	
		/* description unique? */
		for ($i = 1; isset($config['interfaces']['opt' . $i]); $i++) {
			if ($i != $index) {
				if ($config['interfaces']['opt' . $i]['descr'] == $_POST['descr']) {
					$input_errors[] = "An interface with the specified description already exists.";
				}
			}
		}
		
		if ($_POST['bridge']) {
			/* captive portal on? */
			if (isset($config['captiveportal']['enable'])) {
				$input_errors[] = "Interfaces cannot be bridged while the captive portal is enabled.";
			}
		} else {
			$reqdfields = explode(" ", "descr ipaddr subnet");
			$reqdfieldsn = explode(",", "Description,IP address,Subnet bit count");
		
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
				if ($_POST['ipv6ramtu'] < 56 || $_POST['ipv6ramtu'] >1500) {
					$input_errors[] = "A valid RA MTU must be specified (56 - 1500).";
				}
			}
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
		$optcfg['descr'] = $_POST['descr'];
		$optcfg['ipaddr'] = $_POST['ipaddr'];
		$optcfg['subnet'] = $_POST['subnet'];
		$optcfg['bridge'] = $_POST['bridge'];
		$optcfg['enable'] = $_POST['enable'] ? true : false;
		
		if (ipv6enabled()) {
			if ($_POST['raflags'] == "Managed") {
				unset($optcfg['ipv6rao']);
				$optcfg['ipv6ram'] = true;
			} else if ($_POST['raflags'] == "Other") {
				unset($optcfg['ipv6ram']);
				$optcfg['ipv6rao'] = true;
			} else if ($_POST['raflags'] == "None") {
				unset($optcfg['ipv6rao']);
				unset($optcfg['ipv6ram']);
			}
			if ($_POST['ipv6mode'] == "6to4") {
				$optcfg['ipaddr6'] = "6to4";
				unset($optcfg['subnet6']);
				$optcfg['ipv6ra'] = $_POST['ipv6ra'] ? true : false;
				$optcfg['ipv6ram'] = $_POST['ipv6ram'] ? true : false;
				$optcfg['ipv6ramtu'] = $_POST['ipv6ramtu'] ? true : false;
				$optcfg['ipv6rao'] = $_POST['ipv6rao'] ? true : false;
			} else if ($_POST['ipv6mode'] == "DHCP-PD") {
				$optcfg['ipaddr6'] = "DHCP-PD";
				unset($optcfg['subnet6']);
				unset($optcfg['slaid']);
				if ($_POST['slaid'] != null) 
					$optcfg['slaid'] = $_POST['slaid'];
				$optcfg['slalen'] = 64 - $_POST['ispfix'];
				$pconfig['slalen'] = 64 - $_POST['ispfix'];
				$optcfg['ipv6ra'] = $_POST['ipv6ra'] ? true : false;
				$optcfg['ipv6ramtu'] = $_POST['ipv6ramtu'] ;
			} else if ($_POST['ipv6mode'] == "static") {
				$optcfg['ipaddr6'] = $_POST['ipaddr6'];
				$optcfg['subnet6'] = $_POST['subnet6'];
				$optcfg['ipv6ra'] = $_POST['ipv6ra'] ? true : false;
				$optcfg['ipv6ram'] = $_POST['ipv6ram'] ? true : false;
				$optcfg['ipv6ramtu'] = $_POST['ipv6ramtu'] ? true : false;
				$optcfg['ipv6rao'] = $_POST['ipv6rao'] ? true : false;
			} else {
				unset($optcfg['ipaddr6']);
				unset($optcfg['subnet6']);
				unset($optcfg['ipv6ra']);
				unset($optcfg['ipv6ramtu']);
			}
		}

		write_config();
		
		$retval = 0;
		if (!file_exists($d_sysrebootreqd_path)) {
			config_lock();
			$retval = interfaces_optional_configure();
			
			/* is this the captive portal interface? */
			if (isset($config['captiveportal']['enable']) && 
				($config['captiveportal']['interface'] == ('opt' . $index))) {
				captiveportal_configure();
			}
			config_unlock();
		}
		$savemsg = get_std_save_message($retval);
	}
}

$pgtitle = array("Interfaces", "Optional $index (" . htmlspecialchars($optcfg['descr']) . ")");
?>

<?php include("fbegin.inc"); ?>
<script type="text/javascript">
<!--
function enable_change(enable_over) {
	var all_enable = document.iform.enable.checked;
	var bridge_enable = (document.iform.bridge.selectedIndex != 0);
	
	document.iform.descr.disabled = !(all_enable || enable_over);
	document.iform.ipaddr.disabled = !((all_enable && !bridge_enable) || enable_over);
	document.iform.subnet.disabled = !((all_enable && !bridge_enable) || enable_over);
	document.iform.bridge.disabled = !(all_enable || enable_over);
	
<?php if (ipv6enabled()): ?>
	var ipv6_enable = (document.iform.ipv6mode.selectedIndex == 1);
	var pd = (document.iform.ipv6mode.selectedIndex == 3 || enable_over);
	document.iform.ipv6mode.disabled = !((all_enable && !bridge_enable) || enable_over);
	document.iform.ipaddr6.disabled = !((all_enable && !bridge_enable && ipv6_enable) || enable_over);
	document.iform.subnet6.disabled = !((all_enable && !bridge_enable && ipv6_enable) || enable_over);
	document.iform.ipv6ra.disabled = !((all_enable && !bridge_enable && document.iform.ipv6mode.selectedIndex != 0) || enable_over);
	document.iform.ipv6ramtu.disabled = !((all_enable && !bridge_enable && document.iform.ipv6mode.selectedIndex != 0) || enable_over);
	document.iform.raflags.disabled = !((all_enable && !bridge_enable && document.iform.ipv6mode.selectedIndex != 0) || enable_over);
	document.iform.ispfix.disabled = !(pd  && !bridge_enable);
	document.iform.slaid.disabled = !(pd  && !bridge_enable);
<?php endif; ?>

	if (document.iform.mode) {
		 wlan_enable_change(enable_over);
	}
}
//-->
</script>
<?php if (isset($optcfg['wireless'])): ?>
<script type="text/javascript" src="interfaces_wlan.js"></script>
<?php endif; ?>
<?php if ($input_errors) print_input_errors($input_errors); ?>
<?php if ($savemsg) print_info_box($savemsg); ?>
<?php if ($optcfg['if']): ?>
		<table width="100%" border="0" cellpadding="0" cellspacing="0" summary="tab pane">
		  <tr><td class="tabnavtbl">
		  <ul id="tabnav">
			<li class="tabact">Primary configuration</li>
			<li class="tabinact1"><a href="interfaces_secondaries.php?if=opt<?=htmlspecialchars($index)?>&amp;ifname=<?=urlencode("Optional " . htmlspecialchars($index) . " (" . htmlspecialchars($optcfg['descr']) . ")"); ?>">Secondary IPs</a></li>
		  </ul>
		  </td></tr>
		  <tr> 
		<td class="tabcont">
		<table width="100%" border="0" cellpadding="0" cellspacing="0" summary="content pane">
            <form action="interfaces_opt.php" method="post" name="iform" id="iform">
              <table width="100%" border="0" cellpadding="6" cellspacing="0" summary="content pane">
                <tr> 
                  <td width="22%" valign="top" class="vtable">&nbsp;</td>
                  <td width="78%" class="vtable">
<input name="enable" type="checkbox" value="yes" <?php if ($pconfig['enable']) echo "checked"; ?> onClick="enable_change(false)">
                    <strong>Enable Optional <?=htmlspecialchars($index);?> interface</strong></td>
				</tr>
                <tr> 
                  <td width="22%" valign="top" class="vncell">Description</td>
                  <td width="78%" class="vtable"> 
                    <input name="descr" type="text" class="formfld" id="descr" size="30" value="<?=htmlspecialchars($pconfig['descr']);?>">
					<br> <span class="vexpl">Enter a description (name) for the interface here.</span>
				 </td>
				</tr>
                <tr> 
                  <td colspan="2" valign="top" height="16"></td>
				</tr>
				<tr> 
                  <td colspan="2" valign="top" class="listtopic">IP configuration</td>
				</tr>
				<tr> 
                  <td width="22%" valign="top" class="vncellreq">Bridge with</td>
                  <td width="78%" class="vtable">
					<select name="bridge" class="formfld" id="bridge" onChange="enable_change(false)">
				  	<option <?php if (!$pconfig['bridge']) echo "selected";?> value="">none</option>
                      <?php $opts = array('lan' => "LAN", 'wan' => "WAN");
					  	for ($i = 1; isset($config['interfaces']['opt' . $i]); $i++) {
							if ($i != $index)
								$opts['opt' . $i] = "Optional " . $i . " (" .
									$config['interfaces']['opt' . $i]['descr'] . ")";
						}
					foreach ($opts as $opt => $optname): ?>
                      <option <?php if ($opt == $pconfig['bridge']) echo "selected";?> value="<?=htmlspecialchars($opt);?>"> 
                      <?=htmlspecialchars($optname);?>
                      </option>
                      <?php endforeach; ?>
                    </select> </td>
				</tr>
                <tr> 
                  <td width="22%" valign="top" class="vncellreq">IP address</td>
                  <td width="78%" class="vtable"> 
                    <?=$mandfldhtml;?><input name="ipaddr" type="text" class="formfld" id="ipaddr" size="20" value="<?=htmlspecialchars($pconfig['ipaddr']);?>">
                    /
                	<select name="subnet" class="formfld" id="subnet">
					<?php for ($i = 31; $i > 0; $i--): ?>
					<option value="<?=$i;?>" <?php if ($i == $pconfig['subnet']) echo "selected"; ?>><?=$i;?></option>
					<?php endfor; ?>
                    </select>
				 </td>
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
					Choosing 6to4 on an optional interface will make it use the next available /64 prefix within
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
                  <td valign="top" class="vncellreq">IPv6 Prefix Delegation</td>
                  <td class="vtable"> 
                    <input name="slaid" type="text" class="formfld" id="slaid" size="4" value="<?=htmlspecialchars($pconfig['slaid']);?>">
                    / 
                    <select name="ispfix" class="formfld" id="ispfix">
                      <?php for ($l = 64; $l > 0; --$l): ?>
                      <option value="<?=$l;?>" <?php
                        if ($l == 64 - $pconfig['slalen'] || (!isset($pconfig['slalen']) && $l == 48)) echo "selected";
                      ?>>
                      <?=$l;?>
                      </option>
                      <?php endfor; ?>
                    </select><br>
					   Select site-level aggregator ID and ISP prefix length (decimal notation).<br>
					   If ID is 11 and the client is delegated an IPv6 prefix 2001:db8:ffff and prefix 48, this will result in
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
				<?php /* Wireless interface? */
				if (isset($optcfg['wireless']))
					wireless_config_print();
				?>
                <tr> 
                  <td width="22%" valign="top">&nbsp;</td>
                  <td width="78%"> 
                    <input name="index" type="hidden" value="<?=htmlspecialchars($index);?>"> 
				  <input name="Submit" type="submit" class="formbtn" value="Save" onclick="enable_change(true)"> 
                  </td>
                </tr>
                <tr> 
                  <td width="22%" valign="top">&nbsp;</td>
                  <td width="78%"><span class="vexpl"><span class="red"><strong>Note:<br>
                    </strong></span>be sure to add firewall rules to permit traffic 
                    through the interface.	</span></td>
                </tr>
              </table>
</form>
<script type="text/javascript">
<!--
enable_change(false);
<?php if (isset($optcfg['wireless'])): ?>         
wlan_enable_change(false);
<?php endif; ?>
//-->
</script>
<?php else: ?>
<strong>Optional <?=htmlspecialchars($index);?> has been disabled because there is no OPT<?=htmlspecialchars($index);?> interface.</strong>
<?php endif; ?>
</table>
<?php include("fend.inc"); ?>
