#!/usr/local/bin/php
<?php 
/*
	$Id$
	part of m0n0wall (http://m0n0.ch/wall)
	
	Copyright (C) 2007 Marcel Wiget <mwiget@mac.com>
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

$pgtitle = array("Services", "SIP Proxy");
require("guiconfig.inc");

if (!is_array($config['sip'])) {
	$config['sip'] = array();
	$config['sip']['ifin'] = "lan";
	$config['sip']['port'] = "5060";
	$config['sip']['rtplow'] = "7010";
	$config['sip']['rtphigh'] = "7019";
	$config['sip']['debug'] = "0x0000";
	$config['sip']['debugport'] = "0";
}

$pconfig['ifin'] = $config['sip']['ifin'];
$pconfig['port'] = $config['sip']['port'];
$pconfig['rtplow'] = $config['sip']['rtplow'];
$pconfig['rtphigh'] = $config['sip']['rtphigh'];
$pconfig['enable'] = isset($config['sip']['enable']);

if ($_POST) {

	unset($input_errors);
	$pconfig = $_POST;


	/* input validation */
	if ($_POST['enable']) {
		$reqdfields = explode(" ", "port");
		$reqdfieldsn = explode(",", "SIP listening Port");
		
		do_input_validation($_POST, $reqdfields, $reqdfieldsn, &$input_errors);
	}

	if (!$input_errors) {
		$config['sip']['ifin'] = $_POST['ifin'];	
		$config['sip']['port'] = $_POST['port'];
		$config['sip']['rtplow'] = $_POST['rtplow'];
		$config['sip']['rtphigh'] = $_POST['rtphigh'];
		$config['sip']['enable'] = $_POST['enable'] ? true : false;
			
		write_config();

		$retval = 0;
		if (!file_exists($d_sysrebootreqd_path)) {
			config_lock();
			$retval = services_sip_configure();
			$retval = filter_configure();
			config_unlock();
		}
		$savemsg = get_std_save_message($retval);
	}
}

$lancfg = $config['interfaces'][$config['sip']['ifin']];
$lanip = $lancfg['ipaddr'];

?>
<?php include("fbegin.inc"); ?>
<script language="JavaScript">
<!--
function enable_change(enable_change) {
	var endis;
	endis = !(document.iform.enable.checked || enable_change);
	document.iform.ifin.disabled = endis;
	document.iform.port.disabled = endis;
	document.iform.rtplow.disabled = endis;
	document.iform.rtphigh.disabled = endis;
}
//-->
</script>
<?php if ($input_errors) print_input_errors($input_errors); ?>
<?php if ($savemsg) print_info_box($savemsg); ?>
            <form action="services_sip.php" method="post" name="iform" id="iform">
              <table width="100%" border="0" cellpadding="6" cellspacing="0">
                <tr> 
                  <td width="22%" valign="top" class="vtable">&nbsp;</td>
                  <td width="78%" class="vtable">
<input name="enable" type="checkbox" value="yes" <?php if ($pconfig['enable']) echo "checked"; ?> onClick="enable_change(false)">
                    <strong>Enable SIP Proxy</strong>
                    <br> <br>
                  Activates a SIP proxy/masquerading service. Only use it when other 
                  Firewall traversal options like using STUN or outgoing SIP proxy services
                  aren't offered by your SIP service provider. If activated, configure your 
                  SIP User Agents (phone) to use this server as outbound proxy. 
                  </td>
                </tr>
                <tr> 
                  <td width="22%" valign="top" class="vncellreq">Interface</td>
                  <td width="78%" class="vtable">
					<select name="ifin" class="formfld">
                      <?php $interfaces = array('lan' => 'LAN');
					  for ($i = 1; isset($config['interfaces']['opt' . $i]); $i++) {
							$interfaces['opt' . $i] = $config['interfaces']['opt' . $i]['descr'];
					  }
					  foreach ($interfaces as $iface => $ifacename): ?>
                      <option value="<?=$iface;?>" <?php if ($iface == $pconfig['ifin']) echo "selected"; ?>> 
                      <?=htmlspecialchars($ifacename);?>
                      </option>
                      <?php endforeach; ?>
                    </select> 
                    <br>Select the interface local to your SIP endpoints like VOIP phones. Usually your LAN port.
                    </td>
                </tr>
                <tr> 
                  <td width="22%" valign="top" class="vncellreq">SIP UDP port</td>
                  <td width="78%" class="vtable"> 
                    <input name="port" type="text" class="formfld" id="port" size="6" value="<?=htmlspecialchars($pconfig['port']);?>"> 
                  <br>
                  Default UDP port is 5060. If left at default, this proxy also acts
                  as transparent proxy by redirecting outgoing SIP messages to this
                  SIP proxy. 
                  </td>
                </tr>
                <tr> 
                  <td width="22%" valign="top" class="vncellreq">RTP UDP port range</td> 
                  <td width="78%" class="vtable"> 
                    <?=$mandfldhtml;?><input name="rtplow" type="text" class="formfld" id="rtplow" size="6" value="<?=htmlspecialchars($pconfig['rtplow']);?>"> 
                    &nbsp;to&nbsp; <?=$mandfldhtmlspc;?><input name="rtphigh" type="text" class="formfld" id="rtphigh" size="6" value="<?=htmlspecialchars($pconfig['rtphigh']);?>">
                  <br>
                  Range must be large enough to hold multiple concurrent calls. Each audio call needs 2 ports, each video call needs 4 ports.</td>
                </tr>
                <tr> 
                  <td width="22%" valign="top">&nbsp;</td>
                  <td width="78%"> 
                    <input name="Submit" type="submit" class="formbtn" value="Save" onClick="enable_change(true)"> 
                  </td>
                </tr>
                <tr> 
                  <td width="22%" valign="top">&nbsp;</td>
                  <td width="78%"> <p><span class="vexpl"><span class="red">
                    <strong>Note:<br> </strong></span>
                    <br><strong>
                    Local SIP proxy/registrar: <?=htmlspecialchars($lanip);?>:<?=htmlspecialchars($pconfig['port']);?>
                    </strong>
                    <br><br>
                    When enabled on port 5060, all outgoing SIP messages are redirected
                    to this SIP proxy.
                    <br>
                    Firewall rules are added automatically to the WAN interface for
                    the UDP SIP signaling and UDP RTP streams to be reachable from
                    the outside world.
                    <br>
                    It is possible to use this service as a very simple SIP registrar
                    (without authentication, but limited to the local LAN subnet).
                    Use the same server for registration and outbound proxy.
                    </p></td>
                </tr>
              </table>
</form>
<script language="JavaScript">
<!--
enable_change(false);
//-->
</script>
<?php include("fend.inc"); ?>
