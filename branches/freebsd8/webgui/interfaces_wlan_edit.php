#!/usr/local/bin/php
<?php 
/*
	$Id$
	part of m0n0wall (http://m0n0.ch/wall)
	
	Copyright (C) 2011 Manuel Kasper <mk@neon1.net>.
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

$pgtitle = array("Interfaces", "Assign network ports", "Edit WLAN");
require("guiconfig.inc");

if (!is_array($config['wlans']['wlan']))
	$config['wlans']['wlan'] = array();

$a_wlans = &$config['wlans']['wlan'];

$portlist = get_interface_list(false, true, false);

/* if there's only one interface, we can "pre-select" it */
if (count($portlist) == 1) {
	$pconfig = array();
	foreach ($portlist as $pn => $pv) {
		$pconfig['if'] = $pn;
		break;
	}
}

$id = $_GET['id'];
if (isset($_POST['id']))
	$id = $_POST['id'];

if (isset($id) && $a_wlans[$id]) {
	$pconfig['if'] = $a_wlans[$id]['if'];
	$pconfig['descr'] = $a_wlans[$id]['descr'];
	$pconfig['standard'] = $a_wlans[$id]['standard'];
	$pconfig['ht'] = isset($a_wlans[$id]['ht']);
	$pconfig['mode'] = $a_wlans[$id]['mode'];
	$pconfig['ssid'] = $a_wlans[$id]['ssid'];
	$pconfig['channel'] = $a_wlans[$id]['channel'];
	$pconfig['txpower'] = $a_wlans[$id]['txpower'];
	$pconfig['wep_enable'] = isset($a_wlans[$id]['wep']['enable']);
	$pconfig['hidessid'] = isset($a_wlans[$id]['hidessid']);
	
	$pconfig['wpamode'] = $a_wlans[$id]['wpa']['mode'];
	$pconfig['wpaversion'] = $a_wlans[$id]['wpa']['version'];
	$pconfig['wpacipher'] = $a_wlans[$id]['wpa']['cipher'];
	$pconfig['wpapsk'] = $a_wlans[$id]['wpa']['psk'];
	$pconfig['radiusip'] = $a_wlans[$id]['wpa']['radius']['server'];
	$pconfig['radiusauthport'] = $a_wlans[$id]['wpa']['radius']['authport'];
	$pconfig['radiusacctport'] = $a_wlans[$id]['wpa']['radius']['acctport'];
	$pconfig['radiussecret'] = $a_wlans[$id]['wpa']['radius']['secret'];
	
	if (is_array($a_wlans[$id]['wep']['key'])) {
		$i = 1;
		foreach ($a_wlans[$id]['wep']['key'] as $wepkey) {
			$pconfig['key' . $i] = $wepkey['value'];
			if (isset($wepkey['txkey']))
				$pconfig['txkey'] = $i;
			$i++;
		}
		if (!isset($wepkey['txkey']))
			$pconfig['txkey'] = 1;
	}
}

if ($_POST) {

	unset($input_errors);
	$pconfig = $_POST;

	if (!$_POST['chgifonly']) {
		/* input validation */
		$reqdfields = "if mode ssid channel txpower";
		$reqdfieldsn = "Parent interface,Mode,SSID,Channel,Transmit power";
	
		if ($_POST['wpamode'] != "none") {
			$reqdfields .= " wpaversion wpacipher";
			$reqdfieldsn .= ",WPA version,WPA cipher";
		
			if ($_POST['wpamode'] == "psk") {
				$reqdfields .= " wpapsk";
				$reqdfieldsn .= ",WPA PSK";
			} else if ($_POST['wpamode'] == "enterprise") {
				$reqdfields .= " radiusip radiussecret";
				$reqdfieldsn .= ",RADIUS IP,RADIUS Shared secret";
			}
		}
	
		do_input_validation($_POST, explode(" ", $reqdfields), explode(",", $reqdfieldsn), &$input_errors);
	
		if ($_POST['radiusip'] && !is_ipaddr($_POST['radiusip']))
			$input_errors[] = "A valid RADIUS IP address must be specified.";
		if ($_POST['radiusauthport'] && !is_port($_POST['radiusauthport']))
			$input_errors[] = "A valid RADIUS authentication port number must be specified.";
		if ($_POST['radiusacctport'] && !is_port($_POST['radiusacctport']))
			$input_errors[] = "A valid RADIUS accounting port number must be specified.";

		if ($_POST['wpapsk'] && !(strlen($_POST['wpapsk']) >= 8 && strlen($_POST['wpapsk']) <= 63))
			$input_errors[] = "The WPA PSK must be between 8 and 63 characters long.";
	
		/* XXX - check whether multiple WLANs are supported on the physical interface */

		if (!$input_errors) {
			/* if an 11a channel is selected, the mode must be 11a too, and vice versa */
			$is_chan_11a = (strpos($wlchannels[$_POST['channel']]['mode'], "11a") !== false);
			$is_std_11a = ($_POST['standard'] == "11a" || $_POST['standard'] == "11na");
			if ($is_chan_11a && !$is_std_11a)
				$input_errors[] = "802.11a channels can only be selected if the standard is set to 802.11(n)a too.";
			else if (!$is_chan_11a && $is_std_11a)
				$input_errors[] = "802.11(n)a can only be selected if an 802.11a channel is selected too.";
		
			/* currently, WPA is only supported in AP (hostap) mode */
			if ($_POST['wpamode'] != "none" && $pconfig['mode'] != "hostap")
				$input_errors[] = "WPA is only supported in hostap mode.";
		}

		if (!$input_errors) {
			$wlan = array();
			$wlan['if'] = $_POST['if'];
			$wlan['descr'] = $_POST['descr'];
		
			$wlan['standard'] = $_POST['standard'];
			$wlan['ht'] = $_POST['ht'] ? true : false;
			$wlan['mode'] = $_POST['mode'];
			$wlan['ssid'] = $_POST['ssid'];
			$wlan['channel'] = $_POST['channel'];
			$wlan['txpower'] = $_POST['txpower'];
			$wlan['wep']['enable'] = $_POST['wep_enable'] ? true : false;
			$wlan['hidessid'] = $_POST['hidessid'] ? true : false;
		
			$wlan['wep']['key'] = array();
			for ($i = 1; $i <= 4; $i++) {
				if ($_POST['key' . $i]) {
					$newkey = array();
					$newkey['value'] = $_POST['key' . $i];
					if ($_POST['txkey'] == $i)
						$newkey['txkey'] = true;
					$wlan['wep']['key'][] = $newkey;
				}
			}
		
			$wlan['wpa'] = array();
			$wlan['wpa']['mode'] = $_POST['wpamode'];
			$wlan['wpa']['version'] = $_POST['wpaversion'];
			$wlan['wpa']['cipher'] = $_POST['wpacipher'];
			$wlan['wpa']['psk'] = $_POST['wpapsk'];
			$wlan['wpa']['radius'] = array();
			$wlan['wpa']['radius']['server'] = $_POST['radiusip'];
			$wlan['wpa']['radius']['authport'] = $_POST['radiusauthport'];
			$wlan['wpa']['radius']['acctport'] = $_POST['radiusacctport'];
			$wlan['wpa']['radius']['secret'] = $_POST['radiussecret'];

			if (isset($id) && $a_wlans[$id])
				$a_wlans[$id] = $wlan;
			else
				$a_wlans[] = $wlan;
		
			write_config();		
			touch($d_sysrebootreqd_path);
			header("Location: interfaces_wlan.php");
			exit;
		}
	}
}

/* If a physical interface has been selected at this point, we can
   determine the supported standards and channels */
if ($pconfig['if']) {
	$wlstandards = wireless_get_standards($pconfig['if']);
	$wlchannels = wireless_get_channellist($pconfig['if']);
} else {
	$wlstandards = array();
	$wlchannels = array();
}

if (!isset($pconfig['txpower']) || strlen($pconfig['txpower']) == 0)
	$pconfig['txpower'] = 17;	/* pick a reasonable default */

function wireless_get_standards($if) {
	$standards = array();
	
	$fd = popen("/sbin/ifconfig -m $if", "r");
	
	if ($fd) {
		while (!feof($fd)) {
			$line = trim(fgets($fd));
			
			if (preg_match("/media \S+ mode (11\S+)/", $line, $matches)) {
				$standards[] = $matches[1];
			}
		}
	}
	
	return array_unique($standards);
}

function wireless_get_channellist($if) {
	
	$chanlist = array();
	
	/* cannot get channels from physical interface anymore - need a
	   subinterface. Find out if there is already one for this physical
	   interface; if not, create one and destroy it again afterwards */
	unset($subif);
	$fd = popen("/sbin/sysctl -a net.wlan", "r");
	if ($fd) {
		while (!feof($fd)) {
			$line = trim(fgets($fd));
			if (preg_match("/^net\.wlan\.(\d+)\.%parent: (\S+)$/", $line, $matches)) {
				if ($matches[2] == $if) {
					$subif = "wlan" . $matches[1];
					break;
				}
			}
		}
		pclose($fd);
	}
	
	if (!isset($subif)) {
		$subif = "wlan999";
		mwexec("/sbin/ifconfig wlan999 create wlandev " . escapeshellarg($if));
	}
	
	$fd = popen("/sbin/ifconfig $subif list chan", "r");
	if ($fd) {
		while (!feof($fd)) {
			$line = trim(fgets($fd));
			
			/* could have two channels on this line */
			$chans = explode("Channel", $line);
			
			foreach ($chans as $chan) {
				if (preg_match("/(\d+)\s+:\s+(\d+)\s+MHz\s+(.+)/i", $chan, $matches)) {
					$chaninfo = array();
					$chaninfo['chan'] = $matches[1];
					$chaninfo['freq'] = $matches[2];
					$chaninfo['mode'] = trim($matches[3]);
					
					$chanlist[$chaninfo['chan']] = $chaninfo;
				}
			}
		}
		pclose($fd);
	}
	
	if ($subif == "wlan999")
		mwexec("/sbin/ifconfig wlan999 destroy");
	
	ksort($chanlist, SORT_NUMERIC);
	
	return $chanlist;
}

?>
<?php include("fbegin.inc"); ?>
<script type="text/javascript">
<!--
function wlan_enable_change(enable_over) {
    
    // HT only in 11n mode
    var ht_enable = (document.iform.standard.options[document.iform.standard.selectedIndex].value.indexOf("11n") > -1) || enable_over;
	
	// WPA only in hostap mode
	var wpa_enable = (document.iform.mode.options[document.iform.mode.selectedIndex].value == "hostap") || enable_over;
	var wpa_opt_enable = (document.iform.wpamode.options[document.iform.wpamode.selectedIndex].value != "none") || enable_over;
	
	// enter WPA PSK only in PSK mode
	var wpa_psk_enable = (document.iform.wpamode.options[document.iform.wpamode.selectedIndex].value == "psk") || enable_over;
	
	// RADIUS server only in WPA Enterprise mode
	var wpa_ent_enable = (document.iform.wpamode.options[document.iform.wpamode.selectedIndex].value == "enterprise") || enable_over;
	
	// WEP only if WPA is disabled
	var wep_enable = (document.iform.wpamode.options[document.iform.wpamode.selectedIndex].value == "none") || enable_over;
	
	var wep_key_enable = document.iform.wep_enable.checked;
	document.iform.wep_enable.disabled = !wep_enable;
	document.iform.key1.disabled = !wep_key_enable;
	document.iform.key2.disabled = !wep_key_enable;
	document.iform.key3.disabled = !wep_key_enable;
	document.iform.key4.disabled = !wep_key_enable;
	
	document.iform.wpaversion.disabled = !wpa_opt_enable;
	document.iform.wpacipher.disabled = !wpa_opt_enable;
	document.iform.wpapsk.disabled = !wpa_psk_enable;
	document.iform.radiusip.disabled = !wpa_ent_enable;
	document.iform.radiusauthport.disabled = !wpa_ent_enable;
	document.iform.radiusacctport.disabled = !wpa_ent_enable;
	document.iform.radiussecret.disabled = !wpa_ent_enable;
	
	document.iform.ht.disabled = !ht_enable;
}
//-->
</script>
<?php if ($input_errors) print_input_errors($input_errors); ?>
            <form action="interfaces_wlan_edit.php" method="post" name="iform" id="iform">
              <table width="100%" border="0" cellpadding="6" cellspacing="0" summary="content pane">
				<tr>
                  <td width="22%" valign="top" class="vncellreq">Parent interface</td>
                  <td width="78%" class="vtable"> 
                    <select name="if" class="formfld" onchange="document.iform.chgifonly.value='1'; document.iform.submit()">
                      <?php
					  foreach ($portlist as $ifn => $ifinfo): ?>
                      <option value="<?=htmlspecialchars($ifn);?>" <?php if ($ifn == $pconfig['if']) echo "selected"; ?>> 
					  <?php if ($ifinfo['drvname'])
							echo htmlspecialchars($ifn . " (" . $ifinfo['drvname'] . ", " .  $ifinfo['mac'] . ")");
						else
							echo htmlspecialchars($ifn . " (" . $ifinfo['mac'] . ")");
					  ?>
                      </option>
                      <?php endforeach; ?>
                    </select></td>
                </tr>
				<tr>
                  <td width="22%" valign="top" class="vncell">Description</td>
                  <td width="78%" class="vtable"> 
                    <input name="descr" type="text" class="formfld" id="descr" size="40" value="<?=htmlspecialchars($pconfig['descr']);?>">
                    <br> <span class="vexpl">You may enter a description here
                    for your reference (not parsed).</span></td>
                </tr>
                <tr>
                  <td valign="top" class="vncellreq">Standard</td>
                  <td class="vtable"><select name="standard" class="formfld" id="standard" onChange="wlan_enable_change(false)">
                      <?php
					  foreach ($wlstandards as $sn): ?>
                      <option value="<?=htmlspecialchars($sn);?>" <?php if ($sn == $pconfig['standard']) echo "selected";?>>
                      <?="802." . htmlspecialchars($sn);?>
                      </option>
                      <?php endforeach; ?>
                    </select></td>
                </tr>
                <tr>
                  <td valign="top" class="vncellreq">HT</td>
                  <td class="vtable"><input type="checkbox" name="ht" id="ht" value="1" <?php if ($pconfig['ht']) echo "checked";?>><strong>Enable HT</strong></td>
                </tr>
                <tr> 
                  <td valign="top" class="vncellreq">Mode</td>
                  <td class="vtable"><select name="mode" class="formfld" id="mode" onChange="wlan_enable_change(false)">
                      <?php 
						$opts = array();
						if (preg_match($g['wireless_hostap_regex'], $pconfig['if']))
							$opts['hostap'] = "AP";
						$opts['bss'] = "BSS (Station)";
						$opts['ibss'] = "IBSS (ad-hoc)";
					  foreach ($opts as $optn => $optv): ?>
                      <option value="<?=$optn?>" <?php if ($optn == $pconfig['mode']) echo "selected";?>> 
                      <?=htmlspecialchars($optv);?>
                      </option>
                      <?php endforeach; ?>
                    </select></td>
				</tr>
                <tr> 
                  <td valign="top" class="vncellreq">SSID</td>
                  <td class="vtable"><?=$mandfldhtml;?><input name="ssid" type="text" class="formfld" id="ssid" size="20" value="<?=htmlspecialchars($pconfig['ssid']);?>">
                    <br><br><input type="checkbox" name="hidessid" id="hidessid" value="1" <?php if ($pconfig['hidessid']) echo "checked";?>><strong>Hide SSID</strong><br>
                    If this option is selected, the SSID will not be broadcast in AP mode, and only clients that
                    know the exact SSID will be able to connect. Note that this option should never be used
                    as a substitute for proper security/encryption settings.
                  </td>
                </tr>
                <tr> 
                  <td valign="top" class="vncellreq">Channel</td>
                  <td class="vtable"><select name="channel" class="formfld" id="channel">
                      <option <?php if ($pconfig['channel'] == 0) echo "selected";?> value="0">Auto</option>
                      <?php
					  foreach ($wlchannels as $channel => $chaninfo):
					    if ($chaninfo['mode'] == "11g")
					    	$mode = "11b/g";
					    else
					    	$mode = $chaninfo['mode'];
					    
					  	$chandescr = "{$chaninfo['chan']} ({$chaninfo['freq']} MHz, {$mode})";
					  ?>
                      <option <?php if ($channel == $pconfig['channel']) echo "selected";?> value="<?=$channel;?>">
                      <?=$chandescr;?>
                      </option>
                      <?php endforeach; ?>
                    </select></td>
                </tr>
                <tr> 
                  <td valign="top" class="vncellreq">Transmit power</td>
                  <td class="vtable"><select name="txpower" class="formfld" id="txpower">
                      <?php foreach ($wlan_txpowers as $txpower => $txpowerinfo): ?>
                      <option <?php if ($txpower == $pconfig['txpower']) echo "selected";?> value="<?=$txpower;?>">
                      <?=$txpowerinfo;?>
                      </option>
                      <?php endforeach; ?>
                    </select><br>
					Not all transmit power settings listed above may actually be supported by the adapter; in that case the closest
					possible setting will be used. You must ensure that the setting you choose complies with local regulations.</td>
                </tr>
                <tr> 
                  <td valign="top" class="vncell">WPA</td>
                  <td class="vtable">
                    <table width="100%" border="0" cellpadding="6" cellspacing="0">
						<tr> 
							<td colspan="2" valign="top" class="optsect_t2">WPA settings</td>
						</tr>
						<tr>
							<td class="vncell" valign="top">Mode</td>
							<td class="vtable">
								<select name="wpamode" id="wpamode" onChange="wlan_enable_change(false)">
									<option value="none" <?php if (!$pconfig['wpamode'] || $pconfig['wpamode'] == "none") echo "selected";?>>none</option>
									<option value="psk" <?php if ($pconfig['wpamode'] == "psk") echo "selected";?>>PSK</option>
									<option value="enterprise" <?php if ($pconfig['wpamode'] == "enterprise") echo "selected";?>>Enterprise</option>
								</select>
							</td>
						</tr>
						<tr>
							<td class="vncell" valign="top">Version</td>
							<td class="vtable">
								<select name="wpaversion" id="wpaversion">
									<option value="1" <?php if ($pconfig['wpaversion'] == "1") echo "selected";?>>WPA only</option>
									<option value="2" <?php if ($pconfig['wpaversion'] == "2") echo "selected";?>>WPA2 only</option>
									<option value="3" <?php if ($pconfig['wpaversion'] == "3") echo "selected";?>>WPA + WPA2</option>
								</select><br>
								In most cases, you should select &quot;WPA + WPA2&quot; here.
							</td>
						</tr>
						<tr>
							<td class="vncell" valign="top">Cipher</td>
							<td class="vtable">
								<select name="wpacipher" id="wpacipher">
									<option value="tkip" <?php if ($pconfig['wpacipher'] == "tkip") echo "selected";?>>TKIP</option>
									<option value="ccmp" <?php if ($pconfig['wpacipher'] == "ccmp") echo "selected";?>>AES/CCMP</option>
									<option value="both" <?php if ($pconfig['wpacipher'] == "both") echo "selected";?>>TKIP + AES/CCMP</option>
								</select><br>
								AES/CCMP provides better security than TKIP, but TKIP is more compatible with older hardware.
							</td>
						</tr>
						<tr>
							<td class="vncell" valign="top">PSK</td>
							<td class="vtable">
								<input name="wpapsk" type="text" class="formfld" id="wpapsk" size="30" value="<?=htmlspecialchars($pconfig['wpapsk']);?>"><br>
									Enter the passphrase that will be used in WPA-PSK mode. This must be between 8 and 63 characters long.
							</td>
						</tr>
						<tr> 
						  <td colspan="2" class="list" height="12"></td>
						</tr>
						<tr> 
							<td colspan="2" valign="top" class="optsect_t2">RADIUS server</td>
						</tr>
						<tr>
							<td class="vncell" valign="top">IP address</td>
							<td class="vtable"><input name="radiusip" type="text" class="formfld" id="radiusip" size="20" value="<?=htmlspecialchars($pconfig['radiusip']);?>"><br>
							Enter the IP address of the RADIUS server that will be used in WPA-Enterprise mode.</td>
						</tr>
						<tr>
							<td class="vncell" valign="top">Authentication port</td>
							<td class="vtable"><input name="radiusauthport" type="text" class="formfld" id="radiusauthport" size="5" value="<?=htmlspecialchars($pconfig['radiusauthport']);?>"><br>
							 Leave this field blank to use the default port (1812).</td>
						</tr>
						<tr>
							<td class="vncell" valign="top">Accounting port</td>
							<td class="vtable"><input name="radiusacctport" type="text" class="formfld" id="radiusacctport" size="5" value="<?=htmlspecialchars($pconfig['radiusacctport']);?>"><br>
							 Leave this field blank to use the default port (1813).</td>
						</tr>
						<tr>
							<td class="vncell" valign="top">Shared secret&nbsp;&nbsp;</td>
							<td class="vtable"><input name="radiussecret" type="text" class="formfld" id="radiussecret" size="16" value="<?=htmlspecialchars($pconfig['radiussecret']);?>"></td>
						</tr>
					</table>
                  </td>
                </tr>
                <tr> 
                  <td valign="top" class="vncell">WEP</td>
                  <td class="vtable"> <input name="wep_enable" type="checkbox" id="wep_enable" value="yes" <?php if ($pconfig['wep_enable']) echo "checked"; ?> onChange="wlan_enable_change(false)"> 
                    <strong>Enable WEP</strong>
                    <table border="0" cellspacing="0" cellpadding="0">
                      <tr> 
                        <td>&nbsp;</td>
                        <td>&nbsp;</td>
                        <td>&nbsp;TX key&nbsp;</td>
                      </tr>
                      <tr> 
                        <td>Key 1:&nbsp;&nbsp;</td>
                        <td> <input name="key1" type="text" class="formfld" id="key1" size="30" value="<?=htmlspecialchars($pconfig['key1']);?>"></td>
                        <td align="center"> <input name="txkey" type="radio" value="1" <?php if ($pconfig['txkey'] == 1) echo "checked";?>> 
                        </td>
                      </tr>
                      <tr> 
                        <td>Key 2:&nbsp;&nbsp;</td>
                        <td> <input name="key2" type="text" class="formfld" id="key2" size="30" value="<?=htmlspecialchars($pconfig['key2']);?>"></td>
                        <td align="center"> <input name="txkey" type="radio" value="2" <?php if ($pconfig['txkey'] == 2) echo "checked";?>></td>
                      </tr>
                      <tr> 
                        <td>Key 3:&nbsp;&nbsp;</td>
                        <td> <input name="key3" type="text" class="formfld" id="key3" size="30" value="<?=htmlspecialchars($pconfig['key3']);?>"></td>
                        <td align="center"> <input name="txkey" type="radio" value="3" <?php if ($pconfig['txkey'] == 3) echo "checked";?>></td>
                      </tr>
                      <tr> 
                        <td>Key 4:&nbsp;&nbsp;</td>
                        <td> <input name="key4" type="text" class="formfld" id="key4" size="30" value="<?=htmlspecialchars($pconfig['key4']);?>"></td>
                        <td align="center"> <input name="txkey" type="radio" value="4" <?php if ($pconfig['txkey'] == 4) echo "checked";?>></td>
                      </tr>
                    </table>
                    <br>
                    40 (64) bit keys may be entered as 5 ASCII characters or 10 
                    hex digits preceded by '0x'.<br>
                    104 (128) bit keys may be entered as 13 ASCII characters or 
                    26 hex digits preceded by '0x'.</td>
                </tr>
                <tr>
                  <td width="22%" valign="top">&nbsp;</td>
                  <td width="78%"> 
                    <input name="Submit" type="submit" class="formbtn" value="Save">
                    <?php if (isset($id) && $a_wlans[$id]): ?>
                    <input name="id" type="hidden" value="<?=htmlspecialchars($id);?>">
                    <?php endif; ?>
					<input name="chgifonly" type="hidden" value="">
                  </td>
                </tr>
              </table>
</form>
<script type="text/javascript">
<!--
wlan_enable_change(false);
//-->
</script>
<?php include("fend.inc"); ?>
