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

$pgtitle = array("Services", "DHCP server");
require("guiconfig.inc");

list($dnsconfig['dns1'],$dnsconfig['dns2'],$dnsconfig['dns3']) = $config['system']['dnsserver'];

$if = $_GET['if'];
if ($_POST['if'])
	$if = $_POST['if'];
	
$iflist = array("lan" => "LAN");

for ($i = 1; isset($config['interfaces']['opt' . $i]); $i++) {
	$oc = $config['interfaces']['opt' . $i];
	
	if (isset($oc['enable']) && $oc['if'] && (!$oc['bridge'])) {
		$iflist['opt' . $i] = $oc['descr'];
	}
}

if (!$if || !isset($iflist[$if]))
	$if = "lan";

$pconfig['range_from'] = $config['dhcpd'][$if]['range']['from'];
$pconfig['range_to'] = $config['dhcpd'][$if]['range']['to'];
$pconfig['deftime'] = $config['dhcpd'][$if]['defaultleasetime'];
$pconfig['maxtime'] = $config['dhcpd'][$if]['maxleasetime'];
list($pconfig['wins1'],$pconfig['wins2']) = $config['dhcpd'][$if]['winsserver'];
$pconfig['enable'] = isset($config['dhcpd'][$if]['enable']);
$pconfig['denyunknown'] = isset($config['dhcpd'][$if]['denyunknown']);
$pconfig['nextserver'] = $config['dhcpd'][$if]['next-server'];
$pconfig['filename'] = $config['dhcpd'][$if]['filename'];

if (ipv6enabled()) {
	$pconfig['v6range_from'] = $config['dhcpd'][$if]['v6range']['from'];
	$pconfig['v6range_to'] = $config['dhcpd'][$if]['v6range']['to'];
	if (!$config['dhcpd'][$if]['v6defaultleasetime']) {
		$pconfig['v6deftime'] = 7200;
	} else {
		$pconfig['v6deftime'] = $config['dhcpd'][$if]['v6defaultleasetime'];
	}
	if (!$config['dhcpd'][$if]['v6maxleasetime']) {
		$pconfig['v6maxtime'] = 86400;
	} else {
		$pconfig['v6maxtime'] = $config['dhcpd'][$if]['v6maxleasetime'];
	}
	$pconfig['v6enable'] = isset($config['dhcpd'][$if]['v6enable']);
}

$ifcfg = $config['interfaces'][$if];

if (!is_array($config['dhcpd'][$if]['staticmap'])) {
	$config['dhcpd'][$if]['staticmap'] = array();
}
staticmaps_sort($if);
$a_maps = &$config['dhcpd'][$if]['staticmap'];

if ($_POST) {

	foreach ($_POST as $pn => $pv) {
		if (preg_match("/^del_(\d+)_x$/", $pn, $matches)) {
			$id = $matches[1];
			if ($a_maps[$id]) {
				unset($a_maps[$id]);
				write_config();
				touch($d_staticmapsdirty_path);
				header("Location: services_dhcp.php?if={$if}");
				exit;
			}
		}
	}

	unset($input_errors);
	$pconfig = $_POST;

	/* input validation */
	if ($_POST['enable']) {
		$reqdfields = explode(" ", "range_from range_to");
		$reqdfieldsn = explode(",", "Range begin,Range end");
		
		do_input_validation($_POST, $reqdfields, $reqdfieldsn, &$input_errors);
		
		if (($_POST['range_from'] && !is_ipaddr($_POST['range_from']))) {
			$input_errors[] = "A valid range must be specified.";
		}
		if (($_POST['range_to'] && !is_ipaddr($_POST['range_to']))) {
			$input_errors[] = "A valid range must be specified.";
		}
		if (($_POST['wins1'] && !is_ipaddr($_POST['wins1'])) || ($_POST['wins2'] && !is_ipaddr($_POST['wins2']))) {
			$input_errors[] = "A valid IP address must be specified for the primary/secondary WINS server.";
		}
		if ($_POST['deftime'] && (!is_numericint($_POST['deftime']))) {
			$input_errors[] = "The default lease time must be an integer.";
		}
		if ($_POST['maxtime'] && (!is_numericint($_POST['maxtime']) || ($_POST['maxtime'] <= $_POST['deftime']))) {
			$input_errors[] = "The maximum lease time must be higher than the default lease time.";
		}
		if ($_POST['nextserver'] && !is_ipaddr($_POST['nextserver'])) {
			$input_errors[] = "A valid next server IP address must be specified.";
		}
		if (ipv6enabled() && $pconfig['v6enable']) {		
			if (!is_ipaddr6($_POST['v6range_from'])) {
				$input_errors[] = "A valid IPv6 range must be specified.";
			} else if (ipv6Uncompress($_POST['v6range_from']) == ipv6Uncompress(gen_subnet6($ifcfg['ipaddr6'], $ifcfg['subnet6']))) {
				$input_errors[] = "You cannot use the subnet address as a 'from' address.";
			}
			if (!is_ipaddr6($_POST['v6range_to'])) {
				$input_errors[] = "A valid IPv6 range must be specified.";
			} else if (ipv6Uncompress($_POST['v6range_to']) == ipv6Uncompress(gen_subnet_max6($ifcfg['ipaddr6'], $ifcfg['subnet6']))) {
				$input_errors[] = "You cannot use the broadcast address as a 'to' address.";
			}
			if ($_POST['v6range_from'] == $_POST['v6range_to']) {
				$input_errors[] = "IPv6 from and to are the same.";
			}
			if ($_POST['v6deftime'] && (!is_numericint($_POST['v6deftime']))) {
				$input_errors[] = "The default IPv6 lease time must be an integer.";
			}
			if ($_POST['v6maxtime'] && (!is_numericint($_POST['v6maxtime']) || ($_POST['v6maxtime'] <= $_POST['v6deftime']))) {
				$input_errors[] = "The maximum IPv6 lease time must be higher than the default IPv6 lease time.";
			}
		}		
		if (!isset($config['dnsmasq']['enable'])) {
			if (ipv6enabled() && $pconfig['v6enable'] && (!is_ipaddr6($dnsconfig['dns1']) && !is_ipaddr6($dnsconfig['dns2']) && !is_ipaddr6($dnsconfig['dns3']))) {
				$input_errors[] = "DNS forwarder is disabled and you have no IPv6 DNS servers set, DHCPv6 may not have a valid IPv6 DNS setting to give clients.";
			}
			if ($pconfig['enable'] && (!is_ipaddr($dnsconfig['dns1']) && !is_ipaddr($dnsconfig['dns2']) && !is_ipaddr($dnsconfig['dns3']))) {
				$input_errors[] = "DNS forwarder is disabled and you have no IPv4 DNS servers set, DHCP may not have a valid IPv4 DNS setting to give clients.";
			}
		}
		
		if (!$input_errors) {				
			$input_errors = array_merge($input_errors, check_dhcp_range($ifcfg['ipaddr'], $ifcfg['subnet'], $_POST['range_from'], $_POST['range_to']));
			if (ipv6enabled() && $pconfig['v6enable'])
				$input_errors = array_merge($input_errors, check_v6dhcp_range($ifcfg['ipaddr6'], $ifcfg['subnet6'], $_POST['v6range_from'], $_POST['v6range_to']));
			
			/* make sure that the DHCP Relay isn't enabled on this interface */
			if (isset($config['dhcrelay'][$if]['enable']))
				$input_errors[] = "You must disable the DHCP relay on the {$iflist[$if]} interface before enabling the DHCP server.";
		}
	}

	if (!$input_errors) {
		$config['dhcpd'][$if]['range']['from'] = $_POST['range_from'];
		$config['dhcpd'][$if]['range']['to'] = $_POST['range_to'];
		$config['dhcpd'][$if]['defaultleasetime'] = $_POST['deftime'];
		$config['dhcpd'][$if]['maxleasetime'] = $_POST['maxtime'];
		$config['dhcpd'][$if]['enable'] = $_POST['enable'] ? true : false;
		$config['dhcpd'][$if]['denyunknown'] = $_POST['denyunknown'] ? true : false;
		$config['dhcpd'][$if]['next-server'] = $_POST['nextserver'];
		$config['dhcpd'][$if]['filename'] = $_POST['filename'];
		if (ipv6enabled()) {		
			$config['dhcpd'][$if]['v6range']['from'] = $_POST['v6range_from'];
			$config['dhcpd'][$if]['v6range']['to'] = $_POST['v6range_to'];
			$config['dhcpd'][$if]['v6defaultleasetime'] = $_POST['v6deftime'];
			$config['dhcpd'][$if]['v6maxleasetime'] = $_POST['v6maxtime'];
			$config['dhcpd'][$if]['v6enable'] = $_POST['v6enable'] ? true : false;
		}
		
		unset($config['dhcpd'][$if]['winsserver']);
		if ($_POST['wins1'])
			$config['dhcpd'][$if]['winsserver'][] = $_POST['wins1'];
		if ($_POST['wins2'])
			$config['dhcpd'][$if]['winsserver'][] = $_POST['wins2'];
			
		write_config();
		
		$retval = 0;
		if (!file_exists($d_sysrebootreqd_path)) {
			config_lock();
			$retval = services_dhcpd_configure();
			$retval .= services_dhcp6s_configure();
			config_unlock();
		}
		$savemsg = get_std_save_message($retval);
		
		if ($retval == 0) {
			if (file_exists($d_staticmapsdirty_path))
				unlink($d_staticmapsdirty_path);
		}
	}
}

?>
<?php include("fbegin.inc"); ?>
<script type="text/javascript">
<!--
function enable_change(enable_over) {
	var endis;
	endis = !(document.iform.enable.checked || enable_over);
	
	document.iform.range_from.disabled = endis;
	document.iform.range_to.disabled = endis;
	document.iform.wins1.disabled = endis;
	document.iform.wins2.disabled = endis;
	document.iform.deftime.disabled = endis;
	document.iform.maxtime.disabled = endis;
	document.iform.nextserver.disabled = endis;
	document.iform.filename.disabled = endis;
	
	<?php if (ipv6enabled()): ?>
	endis = !(document.iform.v6enable.checked || enable_over);
	document.iform.v6range_from.disabled = endis;
	document.iform.v6range_to.disabled = endis;
	document.iform.v6deftime.disabled = endis;
	document.iform.v6maxtime.disabled = endis;
	<?php endif; ?>
}

//-->
</script>
<form action="services_dhcp.php" method="post" name="iform" id="iform">
<?php if ($input_errors) print_input_errors($input_errors); ?>
<?php if ($savemsg) print_info_box($savemsg); ?>
<?php if (file_exists($d_staticmapsdirty_path)): ?><p>
<?php print_info_box_np("The static mapping configuration has been changed.<br>You must apply the changes in order for them to take effect.");?><br>
<input name="apply" type="submit" class="formbtn" id="apply" value="Apply changes"></p>
<?php endif; ?>
<table width="100%" border="0" cellpadding="0" cellspacing="0" summary="tab pane">
  <tr><td class="tabnavtbl">
  <ul id="tabnav">
<?php $i = 0; foreach ($iflist as $ifent => $ifname):
	if ($ifent == $if): ?>
    <li class="tabact"><?=htmlspecialchars($ifname);?></li>
<?php else: ?>
    <li class="<?php if ($i == 0) echo "tabinact1"; else echo "tabinact";?>"><a href="services_dhcp.php?if=<?=$ifent;?>"><?=htmlspecialchars($ifname);?></a></li>
<?php endif; ?>
<?php $i++; endforeach; ?>
  </ul>
  </td></tr>
  <tr> 
    <td class="tabcont">
              <table width="100%" border="0" cellpadding="6" cellspacing="0" summary="content pane">
					<tr> 
					  <td colspan="2" valign="top" class="optsect_t">
					  <table border="0" cellspacing="0" cellpadding="0" width="100%" summary="checkbox pane">
					  <tr><td class="optsect_s"><strong>Enable IPv4 DHCP server on 
                          <?=htmlspecialchars($iflist[$if]);?>
                          interface</strong></td>
					  <td align="right" class="optsect_s"><input name="enable" type="checkbox" value="yes" <?php if ($pconfig['enable']) echo "checked"; ?> onClick="enable_change(false)"> <strong>Enable</strong></td></tr>
					  </table></td>
					</tr>
					<tr> 
                        <td width="22%" valign="top" class="vncellreq">Deny unknown clients</td>
                        <td width="78%" class="vtable"> 
                          <input name="denyunknown" type="checkbox" value="yes" <?php if ($pconfig['denyunknown']) echo "checked"; ?>> Only respond to reserved clients listed below.
                        </td>
					</tr>
					  

                        <td width="22%" valign="top" class="vncellreq">Subnet</td>
                        <td width="78%" class="vtable"> 
                          <?=gen_subnet($ifcfg['ipaddr'], $ifcfg['subnet']);?>
                        </td>
                      </tr>
                      <tr> 
                        <td width="22%" valign="top" class="vncellreq">Subnet 
                          mask</td>
                        <td width="78%" class="vtable"> 
                          <?=gen_subnet_mask($ifcfg['subnet']);?>
                        </td>
                      </tr>
                      <tr> 
                        <td width="22%" valign="top" class="vncellreq">Available 
                          range</td>
                        <td width="78%" class="vtable"> 
                          <?=long2ip((ip2long($ifcfg['ipaddr']) & gen_subnet_mask_long($ifcfg['subnet'])) + 1);?>
                          - 
                          <?=long2ip((ip2long($ifcfg['ipaddr']) | (~gen_subnet_mask_long($ifcfg['subnet']))) - 1); ?>
                        </td>
                      </tr>
                      <tr> 
                        <td width="22%" valign="top" class="vncellreq">Range</td>
                        <td width="78%" class="vtable"> 
                          <?=$mandfldhtml;?><input name="range_from" type="text" class="formfld" id="range_from" size="20" value="<?=htmlspecialchars($pconfig['range_from']);?>"> 
                          &nbsp;to&nbsp; <?=$mandfldhtmlspc;?><input name="range_to" type="text" class="formfld" id="range_to" size="20" value="<?=htmlspecialchars($pconfig['range_to']);?>"></td>
                      </tr>
                      <tr> 
                        <td width="22%" valign="top" class="vncell">WINS servers</td>
                        <td width="78%" class="vtable"> 
                          <input name="wins1" type="text" class="formfld" id="wins1" size="20" value="<?=htmlspecialchars($pconfig['wins1']);?>"><br>
                          <input name="wins2" type="text" class="formfld" id="wins2" size="20" value="<?=htmlspecialchars($pconfig['wins2']);?>"></td>
                      </tr>
                      <tr> 
                        <td width="22%" valign="top" class="vncell">Default lease 
                          time</td>
                        <td width="78%" class="vtable"> 
                          <input name="deftime" type="text" class="formfld" id="deftime" size="10" value="<?=htmlspecialchars($pconfig['deftime']);?>">
                          seconds<br>
                          This is used for clients that do not ask for a specific 
                          expiration time.<br>
                          The default is 7200 seconds.</td>
                      </tr>
                      <tr> 
                        <td width="22%" valign="top" class="vncell">Maximum lease 
                          time</td>
                        <td width="78%" class="vtable"> 
                          <input name="maxtime" type="text" class="formfld" id="maxtime" size="10" value="<?=htmlspecialchars($pconfig['maxtime']);?>">
                          seconds<br>
                          This is the maximum lease time for clients that ask 
                          for a specific expiration time.<br>
                          The default is 86400 seconds.</td>
                      </tr>
                      <tr>
                        <td width="22%" valign="top" class="vncell">Next server</td>
                        <td width="78%" class="vtable"> 
                          <input name="nextserver" type="text" class="formfld" id="nextserver" size="20" value="<?=htmlspecialchars($pconfig['nextserver']);?>"><br>
                          Specify the server from which clients should load the boot file. This is
                          usually only needed with PXE booting and some VoIP phones, and can usually
                          be left empty.</td>
                      </tr>
                      <tr>
                        <td width="22%" valign="top" class="vncell">Filename</td>
                        <td width="78%" class="vtable"> 
                          <input name="filename" type="text" class="formfld" id="filename" size="20" value="<?=htmlspecialchars($pconfig['filename']);?>"><br>
                          Specify the name of the boot file on the server above. This is
                          usually only needed with PXE booting and some VoIP phones, and can usually
                          be left empty.</td>
                      </tr>
 <?php if (ipv6enabled()) { ?>            
					  
					 <tr> 
					  <td colspan="2" valign="top" class="optsect_t">
					  <table border="0" cellspacing="0" cellpadding="0" width="100%" summary="checkbox pane">
					  <tr><td class="optsect_s"><strong>Enable IPv6 DHCP server on 
                          <?=htmlspecialchars($iflist[$if]);?>
                          interface</strong></td>
					  <td align="right" class="optsect_s"><input name="v6enable" type="checkbox" value="yes" <?php if ($pconfig['v6enable']) echo "checked"; ?> onClick="enable_change(false)"> <strong>Enable</strong></td></tr>
					  </table></td>
					</tr>
						<tr> 
                        <td width="22%" valign="top" class="vncellreq">Subnet</td>
                        <td width="78%" class="vtable"> 
                          <?=gen_subnet6($ifcfg['ipaddr6'], $ifcfg['subnet6']);?> 
                        </td>
                      </tr>
					  <tr>
						<td width="22%" valign="top" class="vncellreq">IPv6 Range</td>
						<td width="78%" class="vtable"> 
						  <?=$mandfldhtml;?><input name="v6range_from" type="text" class="formfld" id="v6range_from" size="20" value="<?=htmlspecialchars($pconfig['v6range_from']);?>"> 
						  &nbsp;to&nbsp; <?=$mandfldhtmlspc;?><input name="v6range_to" type="text" class="formfld" id="v6range_to" size="20" value="<?=htmlspecialchars($pconfig['v6range_to']);?>"></td>
					  </tr>
					  <tr> 
						<td width="22%" valign="top" class="vncell">IPv6 Default lease 
						  time</td>
						<td width="78%" class="vtable"> 
						  <input name="v6deftime" type="text" class="formfld" id="v6deftime" size="10" value="<?=htmlspecialchars($pconfig['v6deftime']);?>">
						  seconds<br>
						  This is used for IPv6 clients that do not ask for a specific 
						  expiration time.<br>
						  The default is 7200 seconds.</td>
					  </tr>
					  <tr> 
						<td width="22%" valign="top" class="vncell">IPv6 Maximum lease 
						  time</td>
						<td width="78%" class="vtable"> 
						  <input name="v6maxtime" type="text" class="formfld" id="v6maxtime" size="10" value="<?=htmlspecialchars($pconfig['v6maxtime']);?>">
						  seconds<br>
						  This is the maximum lease time for IPv6 clients that ask 
						  for a specific expiration time.<br>
						  The default is 86400 seconds.</td>
					  </tr>
<?php } ?>
					  <tr> 
						<td width="22%" valign="top">&nbsp;</td>
						<td width="78%"> 
						  <input name="if" type="hidden" value="<?=htmlspecialchars($if);?>"> 
						  <input name="Submit" type="submit" class="formbtn" value="Save" onclick="enable_change(true)"> 
						</td>
					  </tr>
                      <tr> 
                        <td width="22%" valign="top">&nbsp;</td>
                        <td width="78%"> <p><span class="vexpl"><span class="red"><strong>Note:<br>
                            </strong></span>The DNS servers entered in <a href="system.php">System: 
                            General setup</a> (or the <a href="services_dnsmasq.php">DNS 
                            forwarder</a>, if enabled) </span><span class="vexpl">will 
                            be assigned to clients by the DHCP server.<br>
                            <br>
                            The DHCP lease table can be viewed on the <a href="diag_dhcp_leases.php">Diagnostics: 
                            DHCP leases</a> page.<br>
                            </span></p></td>
                      </tr>
                    </table>
              <table width="100%" border="0" cellpadding="0" cellspacing="0" summary="mac=mapping widget">
			  		<tr> 
					  <td colspan="4" valign="top" class="optsect_t">
					  <table border="0" cellspacing="0" cellpadding="0" width="100%" summary="checkbox pane">
					  <tr><td class="optsect_s"><strong>Reservations</strong></td></tr>
					  </table></td>
					</tr>
                <tr>
                  <td width="35%" class="listhdrr">MAC address </td>
                  <td width="20%" class="listhdrr">IP address</td>
                  <td width="35%" class="listhdr">Description</td>
                  <td width="10%" class="list"></td>
				</tr>
			  <?php $i = 0; foreach ($a_maps as $mapent): ?>
                <tr>
                  <td class="listlr">
                    <?=htmlspecialchars($mapent['mac']);?>
                  </td>
                  <td class="listr">
                    <?=htmlspecialchars($mapent['ipaddr']);?>&nbsp;
                  </td>
                  <td class="listbg">
                    <?=htmlspecialchars($mapent['descr']);?>&nbsp;
                  </td>
                  <td valign="middle" nowrap class="list"> <a href="services_dhcp_edit.php?if=<?=urlencode($if);?>&amp;id=<?=$i;?>"><img src="e.gif" title="edit mapping" width="17" height="17" border="0" alt="edit mapping"></a>
                     &nbsp;<input name="del_<?=$i;?>" type="image" src="x.gif" width="17" height="17" title="delete mapping" alt="delete mapping" onclick="return confirm('Do you really want to delete this mapping?')"></td>
				</tr>
			  <?php $i++; endforeach; ?>
                <tr> 
                  <td class="list" colspan="3"></td>
                  <td class="list"> <a href="services_dhcp_edit.php?if=<?=urlencode($if);?>"><img src="plus.gif" title="add mapping" width="17" height="17" border="0" alt="add mapping"></a></td>
				</tr>
              </table>
    </td>
  </tr>
</table>
</form>
<script type="text/javascript">
<!--
enable_change(false);
//-->
</script>
<?php include("fend.inc"); ?>
