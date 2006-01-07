#!/usr/local/bin/php
<?php 
/*
	firewall_shaper_pipes_edit.php
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

$a_pipes = &$config['shaper']['pipe'];

$id = $_GET['id'];
if (isset($_POST['id']))
	$id = $_POST['id'];
	
if (isset($id) && $a_pipes[$id]) {
	$pconfig['bandwidth'] = $a_pipes[$id]['bandwidth'];
	$pconfig['delay'] = $a_pipes[$id]['delay'];
	$pconfig['mask'] = $a_pipes[$id]['mask'];
	$pconfig['descr'] = $a_pipes[$id]['descr'];
}

if ($_POST) {
	
	unset($input_errors);
	$pconfig = $_POST;

	/* input validation */
	$reqdfields = explode(" ", "bandwidth");
	$reqdfieldsn = explode(",", "Bandwidth");
	
	do_input_validation($_POST, $reqdfields, $reqdfieldsn, &$input_errors);
	
	if (($_POST['bandwidth'] && !is_numericint($_POST['bandwidth']))) {
		$input_errors[] = "The bandwidth must be an integer.";
	}
	if (($_POST['delay'] && !is_numericint($_POST['delay']))) {
		$input_errors[] = "The delay must be an integer.";
	}

	if (!$input_errors) {
		$pipe = array();
		
		$pipe['bandwidth'] = $_POST['bandwidth'];
		if ($_POST['delay'])
			$pipe['delay'] = $_POST['delay'];
		if ($_POST['mask'])
			$pipe['mask'] = $_POST['mask'];
		$pipe['descr'] = $_POST['descr'];
		
		if (isset($id) && $a_pipes[$id])
			$a_pipes[$id] = $pipe;
		else
			$a_pipes[] = $pipe;
		
		write_config();
		touch($d_shaperconfdirty_path);
		
		header("Location: firewall_shaper_pipes.php");
		exit;
	}
}
?>
<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01 Transitional//EN">
<html>
<head>
<title>m0n0wall webGUI - Firewall: Traffic shaper: Edit pipe</title>
<meta http-equiv="Content-Type" content="text/html; charset=iso-8859-1">
<link href="gui.css" rel="stylesheet" type="text/css">
</head>

<body link="#0000CC" vlink="#0000CC" alink="#0000CC">
<?php include("fbegin.inc"); ?>
<p class="pgtitle">Firewall: Traffic shaper: Edit pipe</p>
<?php if ($input_errors) print_input_errors($input_errors); ?>
<?php if ($savemsg) echo htmlspecialchars($savemsg); ?>
            <form action="firewall_shaper_pipes_edit.php" method="post" name="iform" id="iform">
              <table width="100%" border="0" cellpadding="6" cellspacing="0">
                <tr> 
                  <td width="22%" valign="top" class="vncellreq">Bandwidth</td>
                  <td width="78%" class="vtable"> <input name="bandwidth" type="text" id="bandwidth" size="5" value="<?=htmlspecialchars($pconfig['bandwidth']);?>"> 
                    &nbsp;Kbit/s</td>
                </tr>
                <tr> 
                  <td width="22%" valign="top" class="vncell">Delay</td>
                  <td width="78%" class="vtable"> <input name="delay" type="text" id="delay" size="5" value="<?=htmlspecialchars($pconfig['delay']);?>"> 
                    &nbsp;ms<br> <span class="vexpl">Hint: in most cases, you 
                    should specify 0 here (or leave the field empty)</span></td>
                </tr>
                <tr> 
                  <td width="22%" valign="top" class="vncell">Mask</td>
                  <td width="78%" class="vtable"> <select name="mask" class="formfld">
                      <option value="" <?php if (!$pconfig['mask']) echo "selected"; ?>>none</option>
                      <option value="source" <?php if ($pconfig['mask'] == "source") echo "selected"; ?>>source</option>
                      <option value="destination" <?php if ($pconfig['mask'] == "destination") echo "selected"; ?>>destination</option>
                    </select> <br>
                    <span class="vexpl">If 'source' or 'destination' is chosen, 
                    a dynamic pipe with the bandwidth and delay given above will 
                    be created for each source/destination IP address encountered, 
                    respectively. This makes it possible to easily specify bandwidth 
                    limits per host.</span></td>
                </tr>
                <tr> 
                  <td width="22%" valign="top" class="vncell">Description</td>
                  <td width="78%" class="vtable"> <input name="descr" type="text" class="formfld" id="descr" size="40" value="<?=htmlspecialchars($pconfig['descr']);?>"> 
                    <br> <span class="vexpl">You may enter a description here 
                    for your reference (not parsed).</span></td>
                </tr>
                <tr> 
                  <td width="22%" valign="top">&nbsp;</td>
                  <td width="78%"> <input name="Submit" type="submit" class="formbtn" value="Save"> 
                    <?php if (isset($id) && $a_pipes[$id]): ?>
                    <input name="id" type="hidden" value="<?=$id;?>">
                    <?php endif; ?>
                  </td>
                </tr>
              </table>
</form>
<?php include("fend.inc"); ?>
</body>
</html>
