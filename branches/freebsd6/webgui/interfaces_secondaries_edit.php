#!/usr/local/bin/php
<?php 
/*
	$Id: interfaces_secondaries_edit.php 293 2008-08-18 12:06:14Z mkasper $
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

$pgtitle = array("Interfaces", "Add Secondary IP addresses", "Edit Address");
require("guiconfig.inc");

if (!is_array($config['secondaries']['secondary']))
	$config['secondaries']['secondary'] = array();

$a_secondaries = &$config['secondaries']['secondary'] ;

$id = $_GET['id'];
if (isset($_POST['id']))
	$id = $_POST['id'];

$if = $_GET['if'];
if (isset($_POST['if']))
	$if = $_POST['if'];
	
$ifname = $_GET['ifname'];
if (isset($_POST['ifname']))
	$ifname = $_POST['ifname'];

if (isset($id) && $a_secondaries[$id]) {
	$pconfig['if'] = $if;
	$pconfig['ipaddr'] = $a_secondaries[$id]['ipaddr'];
	$pconfig['prefix'] = $a_secondaries[$id]['prefix'];
	$pconfig['descr'] = $a_secondaries[$id]['descr'];
}


if ($_POST) {

	unset($input_errors);
	$pconfig = $_POST;

	/* input validation */
	$reqdfields = explode(" ", "ipaddr prefix");
	$reqdfieldsn = explode(",", "IP Address,subnet/prefix");
	
	do_input_validation($_POST, $reqdfields, $reqdfieldsn, &$input_errors);
	
	if ($_POST['prefix'] && (!is_numericint($_POST['prefix']) || ($_POST['prefix'] < '1') || ($_POST['prefix'] > '128'))) {
		$input_errors[] = "The subnet/prefix must be an integer between 1 and 128.";
	}

	foreach ($a_secondaries as $secondary) {
		if (isset($id) && ($a_secondaries[$id]) && ($a_secondaries[$id] === $secondary))
			continue;
		
		if (($secondary['ipaddr'] == $_POST['ipaddr']) && ($secondary['prefix'] == $_POST['prefix'])) {
			$input_errors[] = "An IP with the subnet/prefix {$secondary['prefix']} is already defined on this interface.";
			break;
		}	
	}

	if (!$input_errors) {
		$secondary = array();
		$secondary['if'] = $if;
		$secondary['ipaddr'] = $_POST['ipaddr'];
		$secondary['prefix'] = $_POST['prefix'];
		$secondary['descr'] = $_POST['descr'];

		if (isset($id) && $a_secondaries[$id])
			$a_secondaries[$id] = $secondary;
		else
			$a_secondaries[] = $secondary;
		
		write_config();		
		touch($d_sysrebootreqd_path);
		header("Location: interfaces_secondaries.php?if=" . urlencode($if) . "&ifname=" . urlencode($ifname));
		exit;
	}
}
?>
<?php include("fbegin.inc"); ?>
<?php if ($input_errors) print_input_errors($input_errors); ?>
            <form action="interfaces_secondaries_edit.php" method="post" name="iform" id="iform">
              <table width="100%" border="0" cellpadding="6" cellspacing="0" summary="content pane">
				<tr>
                  <td width="22%" valign="top" class="vncellreq">IP address</td>
                  <td width="78%" class="vtable">     
				<?=$mandfldhtml;?><input name="ipaddr" type="text" class="formfld" id="ipaddr" size="20" value="<?=htmlspecialchars($pconfig['ipaddr']);?>">
				/ 
                <select name="prefix" class="formfld" id="prefix">
                  <?php for ($i = (ipv6enabled() ? 128 : 32); $i > 0; $i--): ?>
                  <option value="<?=$i;?>" <?php if ($i == $pconfig['prefix']) echo "selected"; ?>>
                  <?=$i;?>
                  </option>
                  <?php endfor; ?>
                </select>
    			</td>
                </tr>
				<tr>
                  <td width="22%" valign="top" class="vncell">Description</td>
                  <td width="78%" class="vtable"> 
                    <input name="descr" type="text" class="formfld" id="descr" size="40" value="<?=htmlspecialchars($pconfig['descr']);?>">
                    <br> <span class="vexpl">You may enter a description here
                    for your reference (not parsed).</span></td>
                </tr>
                <tr>
                  <td width="22%" valign="top">&nbsp;</td>
                  <td width="78%"> 
                    <input name="Submit" type="submit" class="formbtn" value="Save">
                    <?php if (isset($id) && $a_secondaries[$id]): ?>
                    <input name="id" type="hidden" value="<?=htmlspecialchars($id);?>">
                    <?php endif; ?>
					<?php if (isset($if)): ?>
                    <input name="if" type="hidden" value="<?=htmlspecialchars($if);?>">
                    <?php endif; ?>
					<?php if (isset($ifname)): ?>
                    <input name="ifname" type="hidden" value="<?=htmlspecialchars($ifname);?>">
                    <?php endif; ?>
                  </td>
                </tr>
              </table>
</form>
<?php include("fend.inc"); ?>
