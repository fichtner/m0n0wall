#!/usr/local/bin/php
<?php 
/*
	services_captiveportal.php
	part of m0n0wall (http://m0n0.ch/wall)
	
	Copyright (C) 2003-2004 Manuel Kasper <mk@neon1.net>.
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

if (!is_array($config['captiveportal'])) {
	$config['captiveportal'] = array();
	$config['captiveportal']['page'] = array();
	$config['captiveportal']['timeout'] = 60;
}

if ($_GET['act'] == "viewhtml") {
	echo base64_decode($config['captiveportal']['page']['htmltext']);
	exit;
} else if ($_GET['act'] == "viewerrhtml") {
	echo base64_decode($config['captiveportal']['page']['errtext']);
	exit;
}

$pconfig['cinterface'] = $config['captiveportal']['interface'];
$pconfig['timeout'] = $config['captiveportal']['timeout'];
$pconfig['idletimeout'] = $config['captiveportal']['idletimeout'];
$pconfig['enable'] = isset($config['captiveportal']['enable']);
$pconfig['radacct_enable'] = isset($config['captiveportal']['radacct_enable']);
$pconfig['logoutwin_enable'] = isset($config['captiveportal']['logoutwin_enable']);
$pconfig['radiusip'] = $config['captiveportal']['radiusip'];
$pconfig['radiusport'] = $config['captiveportal']['radiusport'];
$pconfig['radiuskey'] = $config['captiveportal']['radiuskey'];

if ($_POST) {

	unset($input_errors);
	$pconfig = $_POST;

	/* input validation */
	if ($_POST['enable']) {
		$reqdfields = explode(" ", "cinterface");
		$reqdfieldsn = explode(",", "Interface");
		
		do_input_validation($_POST, $reqdfields, $reqdfieldsn, &$input_errors);
		
		/* make sure no interfaces are bridged */
		for ($i = 1; isset($config['interfaces']['opt' . $i]); $i++) {
			$coptif = &$config['interfaces']['opt' . $i];
			if (isset($coptif['enable']) && $coptif['bridge']) {
				$input_errors[] = "The captive portal cannot be used when one or more interfaces are bridged.";
				break;
			}
		}
	}
	
	if ($_POST['timeout'] && (!is_numeric($_POST['timeout']) || ($_POST['timeout'] < 1))) {
		$input_errors[] = "The timeout must be at least 1 minute.";
	}
	if ($_POST['idletimeout'] && (!is_numeric($_POST['idletimeout']) || ($_POST['idletimeout'] < 1))) {
		$input_errors[] = "The idle timeout must be at least 1 minute.";
	}
	if (($_POST['radiusip'] && !is_ipaddr($_POST['radiusip']))) {
		$input_errors[] = "A valid IP address must be specified. [".$_POST['radiusip']."]";
	}
	if (($_POST['radiusport'] && !is_port($_POST['radiusport']))) {
		$input_errors[] = "A valid port number must be specified. [".$_POST['radiusport']."]";
	}

	if (!$input_errors) {
		$config['captiveportal']['interface'] = $_POST['cinterface'];
		$config['captiveportal']['timeout'] = $_POST['timeout'];
		$config['captiveportal']['idletimeout'] = $_POST['idletimeout'];
		$config['captiveportal']['enable'] = $_POST['enable'] ? true : false;
		$config['captiveportal']['radacct_enable'] = $_POST['radacct_enable'] ? true : false;
		$config['captiveportal']['logoutwin_enable'] = $_POST['logoutwin_enable'] ? true : false;
		$config['captiveportal']['radiusip'] = $_POST['radiusip'];
		$config['captiveportal']['radiusport'] = $_POST['radiusport'];
		$config['captiveportal']['radiuskey'] = $_POST['radiuskey'];
		
		/* file upload? */
		if (is_uploaded_file($_FILES['htmlfile']['tmp_name']))
			$config['captiveportal']['page']['htmltext'] = base64_encode(file_get_contents($_FILES['htmlfile']['tmp_name']));
		if (is_uploaded_file($_FILES['errfile']['tmp_name']))
			$config['captiveportal']['page']['errtext'] = base64_encode(file_get_contents($_FILES['errfile']['tmp_name']));
			
		write_config();
		
		$retval = 0;
		if (!file_exists($d_sysrebootreqd_path)) {
			config_lock();
			$retval = captiveportal_configure();
			config_unlock();
		}
		$savemsg = get_std_save_message($retval);
	}
}
?>
<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01 Transitional//EN">
<html>
<head>
<title>m0n0wall webGUI - Services: Captive portal</title>
<meta http-equiv="Content-Type" content="text/html; charset=iso-8859-1">
<link href="gui.css" rel="stylesheet" type="text/css">
<script language="JavaScript">
<!--
function radacct_change() {
	if (document.iform.radacct_enable.checked) {
		document.iform.logoutwin_enable.checked = 1;
	} 
}

function enable_change(enable_change) {
	if (document.iform.enable.checked || enable_change) {
		document.iform.cinterface.disabled = 0;
		document.iform.idletimeout.disabled = 0;
		document.iform.timeout.disabled = 0;
		document.iform.radiusip.disabled = 0;
		document.iform.radiusport.disabled = 0;
		document.iform.radiuskey.disabled = 0;
		document.iform.radacct_enable.disabled = 0;
		document.iform.logoutwin_enable.disabled = 0;
		document.iform.htmlfile.disabled = 0;
		document.iform.errfile.disabled = 0;
	} else {
		document.iform.cinterface.disabled = 1;
		document.iform.idletimeout.disabled = 1;
		document.iform.timeout.disabled = 1;
		document.iform.radiusip.disabled = 1;
		document.iform.radiusport.disabled = 1;
		document.iform.radiuskey.disabled = 1;
		document.iform.radacct_enable.disabled = 1;
		document.iform.logoutwin_enable.disabled = 1;
		document.iform.htmlfile.disabled = 1;
		document.iform.errfile.disabled = 1;
	}
	if (enable_change && document.iform.radacct_enable.checked) {
		document.iform.logoutwin_enable.checked = 1;
	}
}
//-->
</script>
</head>

<body link="#0000CC" vlink="#0000CC" alink="#0000CC">
<?php include("fbegin.inc"); ?>
<p class="pgtitle">Services: Captive portal</p>
<?php if ($input_errors) print_input_errors($input_errors); ?>
<?php if ($savemsg) print_info_box($savemsg); ?>
<form action="services_captiveportal.php" method="post" enctype="multipart/form-data" name="iform" id="iform">
<table width="100%" border="0" cellpadding="0" cellspacing="0">
  <tr><td>
  <ul id="tabnav">
	<li class="tabact">Captive portal</li>
	<li class="tabinact"><a href="services_captiveportal_mac.php">Pass-through MAC</a></li>
	<li class="tabinact"><a href="services_captiveportal_ip.php">Allowed IP addresses</a></li>
  </ul>
  </td></tr>
  <tr>
  <td class="tabcont">
  <table width="100%" border="0" cellpadding="6" cellspacing="0">
	<tr> 
	  <td width="22%" valign="top" class="vtable">&nbsp;</td>
	  <td width="78%" class="vtable">
		<input name="enable" type="checkbox" value="yes" <?php if ($pconfig['enable']) echo "checked"; ?> onClick="enable_change(false)">
		<strong>Enable captive portal </strong></td>
	</tr>
	<tr> 
	  <td width="22%" valign="top" class="vncellreq">Interface</td>
	  <td width="78%" class="vtable">
		<select name="cinterface" class="formfld" id="cinterface">
		  <?php $interfaces = array('lan' => 'LAN');
		  for ($i = 1; isset($config['interfaces']['opt' . $i]); $i++) {
			if (isset($config['interfaces']['opt' . $i]['enable']))
				$interfaces['opt' . $i] = $config['interfaces']['opt' . $i]['descr'];
		  }
		  foreach ($interfaces as $iface => $ifacename): ?>
		  <option value="<?=$iface;?>" <?php if ($iface == $pconfig['cinterface']) echo "selected"; ?>> 
		  <?=htmlspecialchars($ifacename);?>
		  </option>
		  <?php endforeach; ?>
		</select> <br>
		<span class="vexpl">Choose which interface to run the captive portal on.</span></td>
	</tr>
	<tr>
	  <td valign="top" class="vncell">Idle timeout</td>
	  <td class="vtable">
		<input name="idletimeout" type="text" class="formfld" id="idletimeout" size="6" value="<?=htmlspecialchars($pconfig['idletimeout']);?>">
minutes<br>
Clients will be disconnected after this amount of inactivity. They may log in again immediately, though. Leave this field blank for no idle timeout.</td>
	</tr>
	<tr> 
	  <td width="22%" valign="top" class="vncell">Hard timeout</td>
	  <td width="78%" class="vtable"> 
		<input name="timeout" type="text" class="formfld" id="timeout" size="6" value="<?=htmlspecialchars($pconfig['timeout']);?>"> 
		minutes<br>
	  Clients will be disconnected after this amount of time, regardless of activity. They may log in again immediately, though. Leave this field blank for no hard timeout (not recommended unless an idle timeout is set).</td>
	</tr>
	<tr> 
	  <td width="22%" valign="top" class="vncell">Logout popup window</td>
	  <td width="78%" class="vtable"> 
		<input name="logoutwin_enable" type="checkbox" class="formfld" id="logoutwin_enable" value="yes" <?php if($pconfig['logoutwin_enable']) echo "checked"; ?>>
		<br>
	  If enabled, a popup window will appear when clients are allowed through the captive portal. This allows clients to explicitly disconnect themselves before the idle or hard timeout occurs. When RADIUS accounting is  enabled, this option is implied.</td>
	</tr>
	<tr> 
	  <td width="22%" valign="top" class="vncell">RADIUS server</td>
	  <td width="78%" class="vtable"> 
		<table cellpadding="0" cellspacing="0">
		<tr>
		<td>IP address:</td>
		<td><input name="radiusip" type="text" class="formfld" id="radiusip" size="20" value="<?=htmlspecialchars($pconfig['radiusip']);?>"></td>
		</tr><tr>
		<td>Port:</td>
		<td><input name="radiusport" type="text" class="formfld" id="radiusport" size="5" value="<?=htmlspecialchars($pconfig['radiusport']);?>"></td>
		</tr><tr>
		<td>Shared secret:&nbsp;&nbsp;</td>
		<td><input name="radiuskey" type="text" class="formfld" id="radiuskey" size="16" value="<?=htmlspecialchars($pconfig['radiuskey']);?>"> </td>
 		</tr><tr>
 		<td>RADIUS accounting:&nbsp;&nbsp;</td>
 		<td><input name="radacct_enable" type="checkbox" id="radacct_enable" value="yes" <?php if($pconfig['radacct_enable']) echo "checked"; ?> onClick="radacct_change()"></td>
		</tr></table>
 		<br>
 	Enter the IP address and port of the RADIUS server which users of the captive portal have to authenticate against. Leave blank to disable RADIUS authentication. Leave port number blank to use the default port (1812). Leave the RADIUS shared secret blank to not use a RADIUS shared secret. RADIUS accounting packets will also be sent to port 1813 of the RADIUS server if RADIUS accounting is enabled.
	</tr>
	<tr> 
	  <td width="22%" valign="top" class="vncellreq">Portal page contents</td>
	  <td width="78%" class="vtable">    
		<input type="file" name="htmlfile" class="formfld" id="htmlfile"><br>
		<?php if ($config['captiveportal']['page']['htmltext']): ?>
		<a href="?act=viewhtml" target="_blank">View current page</a>                      
		  <br>
		  <br>
		<?php endif; ?>
		  Upload an HTML file for the portal page here (leave blank to keep the current one). Make sure to include a form (POST to the page itself)
with a submit button (name=&quot;accept&quot;). Include the &quot;auth_user&quot; and &quot;auth_pass&quot; input elements if RADIUS authentication is enabled. If RADIUS is enabled and no &quot;auth_user&quot; is present, authentication will always fail. If RADIUS is not enabled, you can omit both these input elements.
Example code for the button:<br>
		  <br><tt>&lt;form method=&quot;post&quot; action=&quot;&quot;&gt;<br>  
		  &nbsp;&nbsp;&nbsp;&lt;input name=&quot;accept&quot; type=&quot;submit&quot; value=&quot;Continue&quot;&gt;<br>
		  &nbsp;&nbsp;&nbsp;&lt;input name=&quot;auth_user&quot; type=&quot;text&quot;&gt;<br>
		  &nbsp;&nbsp;&nbsp;&lt;input name=&quot;auth_pass&quot; type=&quot;password&quot;&gt;<br>
		  &lt;/form&gt;</tt>					</td>
	</tr>
	<tr>
	  <td width="22%" valign="top" class="vncell">Authentication<br>
		error page<br>
		contents</td>
	  <td class="vtable">
		<input name="errfile" type="file" class="formfld" id="errfile"><br>
		<?php if ($config['captiveportal']['page']['errtext']): ?>
		<a href="?act=viewerrhtml" target="_blank">View current page</a>                      
		  <br>
		  <br>
		<?php endif; ?>
The contents of the HTML file that you upload here are displayed when a RADIUS authentication error occurs.</td>
	</tr>
	<tr> 
	  <td width="22%" valign="top">&nbsp;</td>
	  <td width="78%"> 
		<input name="Submit" type="submit" class="formbtn" value="Save" onClick="enable_change(true)"> 
	  </td>
	</tr>
	<tr> 
	  <td width="22%" valign="top">&nbsp;</td>
	  <td width="78%"><span class="vexpl"><span class="red"><strong>Note:<br>
		</strong></span>Changing any settings on this page will disconnect all clients! Don't forget to enable the DHCP server on your captive portal interface! Make sure that the default/maximum DHCP lease time is higher than the timeout entered on this page. Also, the DNS forwarder needs to be enabled for DNS lookups by unauthenticated clients to work. </span></td>
	</tr>
  </table>
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
</body>
</html>
