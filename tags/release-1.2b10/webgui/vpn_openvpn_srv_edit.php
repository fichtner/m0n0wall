#!/usr/local/bin/php
<?php 
/*
	vpn_openvpn_srv_edit.php

	Copyright (C) 2004 Peter Curran (peter@closeconsultants.com).
	Copyright (C) 2005 Peter Allgeyer (allgeyer@web.de).
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

$pgtitle = array("VPN", "OpenVPN", "Edit server");
require("guiconfig.inc");
require_once("openvpn.inc");

if (!is_array($config['ovpn']))
	$config['ovpn'] = array();
if (!is_array($config['ovpn']['server'])){
	$config['ovpn']['server'] =  array();
	$config['ovpn']['server']['tunnel'] = array();
}

$ovpnsrv =& $config['ovpn']['server']['tunnel'];

$id = $_GET['id'];
if (isset($_POST['id']))
	$id = $_POST['id'];

if (isset($id) && $ovpnsrv[$id]) {
	$pconfig = $config['ovpn']['server']['tunnel'][$id];
	if (isset($ovpnsrv[$id]['enable']))
		$pconfig['enable'] = true;
} else {
	/* creating - set defaults */
	$pconfig = array();
	$pconfig['type'] = "tun";
	$pconfig['psh_options'] = array();
	/* Initialise with some sensible defaults */
	if ($config['ovpn']['server']['tunnel'])
		$pconfig['port'] = getnxt_server_port();
	else
		$port = 1194;
	$pconfig['proto'] = 'udp';
	$pconfig['maxcli'] = 25;
	$pconfig['crypto'] = 'BF-CBC';
	$pconfig['dupcn'] = true;
	$pconfig['verb'] = 1;
	$pconfig['enable'] = true;
}


if ($_POST) {

	unset($input_errors);

	/* input validation */
	$reqdfields = explode(" ", "type bind_iface ipblock");
	$reqdfieldsn = explode(",", "Tunnel type,Interface binding,IP address block start");

	do_input_validation($_POST, $reqdfields, $reqdfieldsn, &$input_errors);
	
	/* valid IP */
	if (($_POST['ipblock'] && !is_ipaddr($_POST['ipblock'])))
		$input_errors[] = "A valid IP address must be specified.";
		
	/* valid Port */
	if (($_POST['port'] && !is_port($_POST['port'])))
		$input_errors[] = "The server port must be an integer between 1 and 65535.";
	
	/* check if dynip is set correctly */
	if ($_POST['dynip'] && $_POST['bind_iface'] != 'all')
		$input_errors[] = "Dynamic IP address can only be set with interface binding set to ALL.";

	/* Sort out the cert+key files */
	if (empty($_POST['ca_cert']))
		$input_errors[] = "You must provide a CA certificate file";
	elseif (!strstr($_POST['ca_cert'], "BEGIN CERTIFICATE") || !strstr($_POST['ca_cert'], "END CERTIFICATE"))
		$input_errors[] = "The CA certificate does not appear to be valid.";

	if (empty($_POST['srv_cert']))
		$input_errors[] = "You must provide a server certificate file";
	elseif (!strstr($_POST['srv_cert'], "BEGIN CERTIFICATE") || !strstr($_POST['srv_cert'], "END CERTIFICATE"))
		$input_errors[] = "The server certificate does not appear to be valid.";

	if (empty($_POST['srv_key']))
		$input_errors[] = "You must provide a server key file";
	elseif (!strstr($_POST['srv_key'], "BEGIN RSA PRIVATE KEY") || !strstr($_POST['srv_key'], "END RSA PRIVATE KEY"))
		$input_errors[] = "The server key does not appear to be valid.";

	if (empty($_POST['dh_param']))
		$input_errors[] = "You must provide a DH parameters file";
	elseif (!strstr($_POST['dh_param'], "BEGIN DH PARAMETERS") || !strstr($_POST['dh_param'], "END DH PARAMETERS"))
		$input_errors[] = "The DH parameters do not appear to be valid.";
				
	if (isset($_POST['tlsauth']) && empty($_POST['pre-shared-key']))
		$input_errors[] = "You must provide a pre-shared secret file";
	if (!empty($_POST['pre-shared-key']))
		if (!strstr($_POST['pre-shared-key'], "BEGIN OpenVPN Static key") || !strstr($_POST['pre-shared-key'], "END OpenVPN Static key"))
			$input_errors[] = "Pre-shared secret does not appear to be valid.";
				
	if ($_POST['psh_pingrst'] && $_POST['psh_pingexit'])
		$input_errors[] = "Ping-restart and Ping-exit are mutually exclusive and cannot be used together";

	if ($_POST['psh_rtedelay'] && !is_numeric($_POST['psh_rtedelay_int']))
		$input_errors[] = "Route-delay needs a numerical interval setting.";

	if ($_POST['psh_inact'] && !is_numeric($_POST['psh_inact_int']))
		$input_errors[] = "Inactive needs a numerical interval setting.";

	if ($_POST['psh_ping'] && !is_numeric($_POST['psh_ping_int']))
		$input_errors[] = "Ping needs a numerical interval setting.";
			
	if ($_POST['psh_pingexit'] && !is_numeric($_POST['psh_pingexit_int']))
		$input_errors[] = "Ping-exit needs a numerical interval setting.";

	if ($_POST['psh_pingrst'] && !is_numeric($_POST['psh_pingrst_int']))
		$input_errors[] = "Ping-restart needs a numerical interval setting.";


	/* need a test here to make sure prefix and max_clients are coherent */

	/* need a test here to make sure protocol:ip:port isn't used twice */

	/* Editing an existing entry? */
	if (isset($id) && $ovpnsrv[$id]) {
		$ovpnent = $ovpnsrv[$id];

		if ( $ovpnent['bind_iface'] != $_POST['bind_iface'] ||
		     $ovpnent['port'] != $_POST['port'] ||
		     $ovpnent['proto'] != $_POST['proto'] ) {

			/* some entries changed */
			for ($i = 0; isset($config['ovpn']['server']['tunnel'][$i]); $i++) {
				$current = &$config['ovpn']['server']['tunnel'][$i];

				if ($current['bind_iface'] == $_POST['bind_iface'] || $current['bind_iface'] == 'all')
					if ($current['port'] == $_POST['port'])
						if ($current['proto'] == $_POST['proto'])
							$input_errors[] = "You already have this combination for Interface binding, port and protocol settings. You can't use it twice";
			}
		}

		/* Test Server type hasn't changed */
		if ($ovpnent['type'] != $_POST['type']) {
			$input_errors[] = "Delete this interface first before changing the type of the tunnel to " . strtoupper($_POST['type']) .".";

			/* Temporarily disabled */
			/*
			 * $nxt_if = getnxt_server_if($_POST['type']);
			 * if (!$nxt_if)
			 * 		$input_errors[] = "Run out of devices for a tunnel of type {$_POST['type']}";
			 * else
			 * 	$ovpnent['tun_iface'] = $nxt_if;
			 */
			 /* Need to reboot in order to create interfaces cleanly */
			 /* touch($d_sysrebootreqd_path); */
		}
		/* Has the enable/disable state changed? */
		if (isset($ovpnent['enable']) && isset($_POST['disabled'])) {
			/* status changed to disabled */
			touch($d_ovpnsrvdirty_path);
		}
		if (!isset($ovpnent['enable']) && !isset($_POST['disabled'])) {
			/* status changed to enable */
			/* touch($d_sysrebootreqd_path); */
			touch($d_ovpnsrvdirty_path);
		}
	} else {
		/* Creating a new entry */
		$ovpnent = array();
		$nxt_if = getnxt_server_if($_POST['type']);
		if (!$nxt_if)
			$input_errors[] = "Run out of devices for a tunnel of type {$_POST['type']}";
		else
			$ovpnent['tun_iface'] = $nxt_if;
		$ovpnent['port'] = getnxt_server_port();
		/* I think we have to reboot to have the interface created cleanly */
		touch($d_sysrebootreqd_path);
	}

	if (!$input_errors) {

		$ovpnent['enable'] = isset($_POST['disabled']) ? false : true;
		$ovpnent['bind_iface'] = $_POST['bind_iface'];
		$ovpnent['port'] = $_POST['port'];
		$ovpnent['proto'] = $_POST['proto'];
		$ovpnent['type'] = $_POST['type'];
		
		/* convert IP address block to a correct network IP address */
		$ipblock = gen_subnet($_POST['ipblock'], $_POST['prefix']);
		$ovpnent['ipblock'] = $ipblock;

		$ovpnent['prefix'] = $_POST['prefix'];
		$ovpnent['descr'] = $_POST['descr'];
		$ovpnent['verb'] = $_POST['verb'];
		$ovpnent['maxcli'] = $_POST['maxcli'];
		$ovpnent['crypto'] = $_POST['crypto'];
		$ovpnent['cli2cli'] = $_POST['cli2cli'] ? true : false;
		$ovpnent['dupcn'] = $_POST['dupcn'] ? true : false;
		$ovpnent['dynip'] = $_POST['dynip'] ? true : false;
		$ovpnent['tlsauth'] = false;

		unset($ovpnent['pre-shared-key']);
		if ($_POST['tlsauth']) {
			$ovpnent['tlsauth'] = true;
			$ovpnent['pre-shared-key'] = base64_encode($_POST['pre-shared-key']);	
		}

		$ovpnent['psh_options']['redir'] = $_POST['psh_redir'] ? true : false;
		$ovpnent['psh_options']['redir_loc'] = $_POST['psh_redir_loc'] ? true : false;
		$ovpnent['psh_options']['rtedelay'] = $_POST['psh_rtedelay'] ? true : false;
		$ovpnent['psh_options']['inact'] = $_POST['psh_inact'] ? true : false;
		$ovpnent['psh_options']['ping'] = $_POST['psh_ping'] ? true : false;
		$ovpnent['psh_options']['pingrst'] = $_POST['psh_pingrst'] ? true : false;
		$ovpnent['psh_options']['pingexit'] = $_POST['psh_pingexit'] ? true : false;

		unset($ovpnent['psh_options']['rtedelay_int']);
		unset($ovpnent['psh_options']['inact_int']);
		unset($ovpnent['psh_options']['ping_int']);
		unset($ovpnent['psh_options']['pingrst_int']);
		unset($ovpnent['psh_options']['pingexit_int']);

		if ($_POST['psh_rtedelay_int'])
			$ovpnent['psh_options']['rtedelay_int'] = $_POST['psh_rtedelay_int'];
		if ($_POST['psh_inact_int'])
			$ovpnent['psh_options']['inact_int'] = $_POST['psh_inact_int'];
		if ($_POST['psh_ping_int'])
			$ovpnent['psh_options']['ping_int'] = $_POST['psh_ping_int'];
		if ($_POST['psh_pingrst_int'])
			$ovpnent['psh_options']['pingrst_int'] = $_POST['psh_pingrst_int'];
		if ($_POST['psh_pingexit_int'])
			$ovpnent['psh_options']['pingexit_int'] = $_POST['psh_pingexit_int'];
		
		$ovpnent['ca_cert'] = base64_encode($_POST['ca_cert']);
		$ovpnent['srv_cert'] = base64_encode($_POST['srv_cert']);
		$ovpnent['srv_key'] = base64_encode($_POST['srv_key']);
		$ovpnent['dh_param'] = base64_encode($_POST['dh_param']);	

		if (isset($id) && $ovpnsrv[$id])
			$ovpnsrv[$id] = $ovpnent;
		else
			$ovpnsrv[] = $ovpnent;

		write_config();
		touch($d_ovpnsrvdirty_path);

		header("Location: vpn_openvpn_srv.php");
		exit;
	} else {

		$pconfig = $_POST;

		$pconfig['enable'] = "true";
		if (isset($_POST['disabled']))
			unset($pconfig['enable']);

		if ($_POST['tlsauth'])
			$pconfig['pre-shared-key'] = base64_encode($_POST['pre-shared-key']);	

		$pconfig['ca_cert'] = base64_encode($_POST['ca_cert']);
		$pconfig['srv_cert'] = base64_encode($_POST['srv_cert']);
		$pconfig['srv_key'] = base64_encode($_POST['srv_key']);
		$pconfig['dh_param'] = base64_encode($_POST['dh_param']);

		$pconfig['psh_options']['redir'] = $_POST['psh_redir'];
		$pconfig['psh_options']['redir_loc'] = $_POST['psh_redir_loc'];
		$pconfig['psh_options']['rtedelay'] = $_POST['psh_rtedelay'];
		$pconfig['psh_options']['inact'] = $_POST['psh_inact'];
		$pconfig['psh_options']['ping'] = $_POST['psh_ping'];
		$pconfig['psh_options']['pingrst'] = $_POST['psh_pingrst'];
		$pconfig['psh_options']['pingexit'] = $_POST['psh_pingexit'];

		$pconfig['psh_options']['rtedelay_int'] = $_POST['psh_rtedelay_int'];
		$pconfig['psh_options']['inact_int'] = $_POST['psh_inact_int'];
		$pconfig['psh_options']['ping_int'] = $_POST['psh_ping_int'];
		$pconfig['psh_options']['pingrst_int'] = $_POST['psh_pingrst_int'];
		$pconfig['psh_options']['pingexit_int'] = $_POST['psh_pingexit_int'];
	}
}


?>
<?php include("fbegin.inc"); ?>
<script language="JavaScript">
function type_change() {
	switch (document.iform.bind_iface.selectedIndex) {
		/* ALL */
		case 0:
			document.iform.dynip.disabled = 0;
			break;
		default:
			document.iform.dynip.disabled = 1;
	}
}
function enable_change(enable_over) {
	var endis;
	endis = !(document.iform.tlsauth.checked || enable_over);
        
        document.iform.psk.disabled = endis;
}

//-->
</script>
<?php if ($input_errors) print_input_errors($input_errors); ?>
<?php if (file_exists($d_sysrebootreqd_path)) print_info_box(get_std_save_message(0)); ?>

<form action="vpn_openvpn_srv_edit.php" method="post" enctype="multipart/form-data" name="iform" id="iform">
<strong><span class="red">WARNING: This feature is experimental and modifies your optional interface configuration.
  Backup your configuration before using OpenVPN, and restore it before upgrading.<br>&nbsp;<br>
</span></strong>
<table width="100%" border="0" cellpadding="6" cellspacing="0">
  <tr>
    <td width="22%" valign="top" class="vncellreq">Disabled</td>
    <td width="78%" class="vtable">
      <input name="disabled" type="checkbox" value="yes" <?php if (!isset($pconfig['enable'])) echo "checked"; ?>>
      <strong>Disable this server</strong><br>
        <span class="vexpl">Set this option to disable this server without removing it from the list.</span>
    </td>
   </tr>
   
   <tr>
     <td width="22%" valign="top" class="vncellreq">Tunnel type</td>
     <td width="78%" class="vtable">
       <input type="radio" name="type" class="formfld" value="tun" <?php if ($pconfig['type'] == 'tun') echo "checked"; ?>>
          TUN&nbsp;
       <input type="radio" name="type" class="formfld" value="tap" <?php if ($pconfig['type'] == 'tap') echo "checked"; ?>>
          TAP
      </td>
    </tr>

    <tr>
      <td width="22%" valign="top" class="vncell">OpenVPN protocol/port</td>
      <td width="78%" class="vtable">
	<input type="radio" name="proto" class="formfld" value="udp" <?php if ($pconfig['proto'] == 'udp') echo "checked"; ?>>
           UDP&nbsp;
        <input type="radio" name="proto" class="formfld" value="tcp" <?php if ($pconfig['proto'] == 'tcp') echo "checked"; ?>>
           TCP<br><br>
        Port: 
        <input name="port" type="text" class="formfld" size="5" maxlength="5" value="<?= $pconfig['port']; ?>"><br>
        Enter the port number to use for the server (default is 1194).</td>
    </tr>
    
    <tr>
      <td width="22%" valign="top" class="vncellreq">Interface binding</td>
      <td width="78%" class="vtable">
	<select name="bind_iface" class="formfld" onchange="type_change()">
        <?php 
	$interfaces = ovpn_real_interface_list();
	foreach ($interfaces as $key => $iface):
        ?>
	<option value="<?=$key;?>" <?php if ($key == $pconfig['bind_iface']) echo "selected"; ?>> <?= $iface;?>
        </option>
        <?php endforeach;?>
        </select>
        <span class="vexpl"><br>
        Choose an interface for the OpenVPN server to listen on.</span></td>
    </tr>
		
    <tr>
      <td width="22%" valign="top" class="vncell">Dynamic IP address</td>
      <td width="78%" class="vtable">
	<input name="dynip" type="checkbox" value="yes" <?php if (isset($pconfig['dynip'])) echo "checked"; ?>>
	<strong>Dynamic IP address</strong><br>
	Set this option to on, if your IP addresses are being assigned dynamically. Can only be used with interface binding set to ALL.</td>
    </tr>
	 
    <tr> 
      <td width="22%" valign="top" class="vncellreq">VPN client address pool</td>
      <td width="78%" class="vtable"> 
        <input name="ipblock" type="text" class="formfld" size="20" value="<?=htmlspecialchars($pconfig['ipblock']);?>">
        / 
        <select name="prefix" class="formfld">
          <?php for ($i = 29; $i > 19; $i--): ?>
          <option value="<?=$i;?>" <?php if ($i == $pconfig['prefix']) echo "selected"; ?>>
            <?=$i;?>
          </option>
          <?php endfor; ?>
        </select>
        <br>
        Enter the IP address block for the OpenVPN server and clients to use.<br>
        <br>
	Maximum number of simultaneous clients: 
	<input name="maxcli" type="text" class="formfld" size="3" maxlength="3" value="<?=htmlspecialchars($pconfig['maxcli']);?>">
	</td>
    </tr>

    <tr> 
      <td width="22%" valign="top" class="vncell">Description</td>
      <td width="78%" class="vtable"> 
        <input name="descr" type="text" class="formfld" id="descr" size="40" value="<?=htmlspecialchars($pconfig['descr']);?>"> 
        <br> <span class="vexpl">You may enter a description here for your reference (not parsed).</span></td>
    </tr>
    
    <tr> 
      <td width="22%" valign="top" class="vncellreq">CA certificate</td>
      <td width="78%" class="vtable"> 
      <textarea name="ca_cert" cols="65" rows="4" class="formpre"><?=htmlspecialchars(base64_decode($pconfig['ca_cert']));?></textarea>
      <br>
      Paste a CA certificate in X.509 PEM format here.</td>
    </tr>
		
    <tr> 
      <td width="22%" valign="top" class="vncellreq">Server certificate</td>
      <td width="78%" class="vtable">
        <textarea name="srv_cert" cols="65" rows="4" class="formpre"><?=htmlspecialchars(base64_decode($pconfig['srv_cert']));?></textarea>
        <br>
        Paste a server certificate in X.509 PEM format here.</td>
    </tr>
     
    <tr> 
      <td width="22%" valign="top" class="vncellreq">Server key</td>
      <td width="78%" class="vtable"> 
        <textarea name="srv_key" cols="65" rows="4" class="formpre"><?=htmlspecialchars(base64_decode($pconfig['srv_key']));?></textarea>
        <br>Paste the server RSA private key here.</td>
    </tr>
      
    <tr> 
      <td width="22%" valign="top" class="vncellreq">DH parameters</td>
      <td width="78%" class="vtable"> 
	<textarea name="dh_param" cols="65" rows="4" class="formpre"><?=htmlspecialchars(base64_decode($pconfig['dh_param']));?></textarea>
	<br>          
	  Paste the Diffie-Hellman parameters in PEM format here.</td>
    </tr>
      
    <tr>
      <td width="22%" valign="top" class="vncell">Crypto</td>
      <td width="78%" class="vtable">
	<select name="crypto" class="formfld">
	  <?php $cipher_list = ovpn_get_cipher_list();
		foreach($cipher_list as $key => $value){
	  ?>
		<option value="<?= $key ?>" <?php if ($pconfig['crypto'] == $key) echo "selected"; ?>>
		<?= $value ?>
		</option>
	  <?php
	    }
	  ?>
	  </select>
	  <br>
	Select a data channel encryption cipher.</td>
    </tr>
      
    <tr>
      <td width="22%" valign="top" class="vncell">TLS auth</td>
      <td width="78%" class="vtable">
	<input name="tlsauth" type="checkbox" value="yes" <?php if (isset($pconfig['tlsauth'])) echo "checked";?> onClick="enable_change(false)">
	<strong>TLS auth</strong><br>
	The tls-auth directive adds an additional HMAC signature to all SSL/TLS handshake packets for integrity verification.</td>
    </tr>

    <tr> 
      <td width="22%" valign="top" class="vncell">Pre-shared secret</td>
      <td width="78%" class="vtable">
	<textarea name="pre-shared-key" id="psk" cols="65" rows="4" class="formpre"><?=htmlspecialchars(base64_decode($pconfig['pre-shared-key']));?></textarea>
	<br>
	Paste your own pre-shared secret here.</td>
    </tr>

    <tr>
      <td width="22%" valign="top" class="vncell">Internal routing mode</td>
      <td width="78%" class="vtable">
	<input name="cli2cli" type="checkbox" value="yes" <?php if (isset($pconfig['cli2cli'])) echo "checked"; ?>>
	<strong>Enable client-to-client routing</strong><br>
	If this option is on, clients are allowed to talk to each other.</td>
    </tr>
      
    <tr>
      <td width="22%" valign="top" class="vncell">Client authentication</td>
      <td width="78%" class="vtable">
	<input name="dupcn" type="checkbox" value="yes" <?php if (isset($pconfig['dupcn'])) echo "checked"; ?>>
        <strong>Permit duplicate client certificates</strong><br>
	If this option is on, clients with duplicate certificates will not be disconnected.</td>
    </tr>
	 
    <tr>
      <td width="22%" valign="top" class="vncell">Client-push options</td>
      <td width="78%" class="vtable">
	    <table border="0" cellspacing="0" cellpadding="0">
	      <tr>
            <td><input type="checkbox" name="psh_redir" value="yes" <?php if (isset($pconfig['psh_options']['redir'])) echo "checked"; ?>>
            Redirect-gateway</td>
            <td>&nbsp;</td>
            <td><input type="checkbox" name="psh_redir_loc" value="yes" <?php if (isset($pconfig['psh_options']['redir_loc'])) echo "checked"; ?>>
              Local</td>
	        </tr>
          <tr>
            <td><input type="checkbox" name="psh_rtedelay" value="yes" <?php if (isset($pconfig['psh_options']['rtedelay'])) echo "checked"; ?>> Route-delay</td>
            <td width="16">&nbsp;</td>
            <td><input type="text" name="psh_rtedelay_int" class="formfld" size="4" value="<?= $pconfig['psh_options']['rtedelay_int']?>"> seconds</td>
          </tr>
          <tr>
            <td><input type="checkbox" name="psh_inact" value="yes" <?php if (isset($pconfig['psh_options']['inact'])) echo "checked"; ?>>
    Inactive</td>
            <td>&nbsp;</td>
            <td><input type="text" name="psh_inact_int" class="formfld" size="4" value="<?= $pconfig['psh_options']['inact_int']?>">
    seconds</td>
          </tr>
          <tr>
            <td><input type="checkbox" name="psh_ping" value="yes" <?php if (isset($pconfig['psh_options']['ping'])) echo "checked"; ?>> Ping</td>
            <td>&nbsp;</td>
            <td>Interval: <input type="text" name="psh_ping_int" class="formfld" size="4" value="<?= $pconfig['psh_options']['ping_int']?>"> seconds</td>
          </tr>
          <tr>
            <td><input type="checkbox" name="psh_pingexit" value="yes" <?php if (isset($pconfig['psh_options']['pingexit'])) echo "checked"; ?>> Ping-exit</td>
            <td>&nbsp;</td>
            <td>Interval: <input type="text" name="psh_pingexit_int" class="formfld" size="4" value="<?= $pconfig['psh_options']['pingexit_int']?>"> seconds</td>
          </tr>
          <tr>
            <td><input type="checkbox" name="psh_pingrst" value="yes" <?php if (isset($pconfig['psh_options']['pingrst'])) echo "checked"; ?>> Ping-restart</td>
            <td>&nbsp;</td>
            <td>Interval: <input type="text" name="psh_pingrst_int" class="formfld" size="4" value="<?= $pconfig['psh_options']['pingrst_int']?>"> seconds</td>
          </tr>
        </table></td>
    </tr>
    <tr>
      <td width="22%" valign="top">&nbsp;</td>
      <td width="78%">
        <input name="Submit" type="submit" class="formbtn" value="Save">
        <input name="verb" type="hidden" value="<?=$pconfig['verb'];?>"> 
        <?php if (isset($id)): ?>
        <input name="id" type="hidden" value="<?=$id;?>"> 
        <?php endif; ?>
      </td>
    </tr>
    <tr>
      <td width="22%" valign="top">&nbsp;</td>
      <td width="78%"><span class="vexpl"><span class="red"><strong>Note:<br>
        </strong></span>Changing any settings on this page will disconnect all clients!</span>
      </td>
    </tr>
</table>
</form>
<script language="JavaScript">
<!--
type_change();
enable_change(false);

//-->
</script>
<?php include("fend.inc"); ?>
