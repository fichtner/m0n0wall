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

$pgtitle = array("Interfaces", "Assign network ports");
require("guiconfig.inc");

if (!is_array($config['vlans']['vlan']))
	$config['vlans']['vlan'] = array();

$a_vlans = &$config['vlans']['vlan'] ;

function vlan_inuse($num) {
	global $config, $g;

	if ($config['interfaces']['lan']['if'] == "vlan{$num}")
		return true;
	if ($config['interfaces']['wan']['if'] == "vlan{$num}")
		return true;
	
	for ($i = 1; isset($config['interfaces']['opt' . $i]); $i++) {
		if ($config['interfaces']['opt' . $i]['if'] == "vlan{$num}")
			return true;
	}
	
	return false;
}

function renumber_vlan($if, $delvlan) {
	if (!preg_match("/^vlan/", $if))
		return $if;
	
	$vlan = substr($if, 4);
	if ($vlan > $delvlan)
		return "vlan" . ($vlan - 1);
	else
		return $if;
}

if ($_POST) {
	foreach ($_POST as $pn => $pv) {
		if (preg_match("/^del_(\d+)_x$/", $pn, $matches)) {
			$id = $matches[1];
			/* check if still in use */
			if (vlan_inuse($id)) {
				$input_errors[] = "This VLAN cannot be deleted because it is still being used as an interface.";
			} else {
				unset($a_vlans[$id]);

				/* renumber all interfaces that use VLANs */
				$config['interfaces']['lan']['if'] = renumber_vlan($config['interfaces']['lan']['if'], $id);
				$config['interfaces']['wan']['if'] = renumber_vlan($config['interfaces']['wan']['if'], $id);
				for ($i = 1; isset($config['interfaces']['opt' . $i]); $i++)
					$config['interfaces']['opt' . $i]['if'] = renumber_vlan($config['interfaces']['opt' . $i]['if'], $id);

				write_config();
				touch($d_sysrebootreqd_path);
				header("Location: interfaces_vlan.php");
				exit;
			}
		}
	}
}

?>
<?php include("fbegin.inc"); ?>
<?php if ($input_errors) print_input_errors($input_errors); ?>
<?php if (file_exists($d_sysrebootreqd_path)) print_info_box(get_std_save_message(0)); ?>
<form action="interfaces_vlan.php" method="post" name="iform" id="iform">
<table width="100%" border="0" cellpadding="0" cellspacing="0" summary="tab pane">
  <tr><td class="tabnavtbl">
  <ul id="tabnav">
    <li class="tabinact1"><a href="interfaces_assign.php">Interface assignments</a></li>
    <li class="tabact">VLANs</li>
  </ul>
  </td></tr>
  <tr> 
    <td class="tabcont">
<table width="100%" border="0" cellpadding="0" cellspacing="0" summary="content pane">
                <tr>
                  <td width="20%" class="listhdrr">Interface</td>
                  <td width="20%" class="listhdrr">VLAN tag</td>
                  <td width="50%" class="listhdr">Description</td>
                  <td width="10%" class="list"></td>
				</tr>
			  <?php $i = 0; foreach ($a_vlans as $vlan): ?>
                <tr>
                  <td class="listlr">
					<?=htmlspecialchars($vlan['if']);?>
                  </td>
                  <td class="listr">
					<?=htmlspecialchars($vlan['tag']);?>
                  </td>
                  <td class="listbg">
                    <?=htmlspecialchars($vlan['descr']);?>&nbsp;
                  </td>
                  <td valign="middle" nowrap class="list"> <a href="interfaces_vlan_edit.php?id=<?=$i;?>"><img src="e.gif" title="edit VLAN" width="17" height="17" border="0" alt="edit VLAN"></a>
                     &nbsp;<input name="del_<?=$i;?>" type="image" src="x.gif" width="17" height="17" title="delete VLAN" alt="delete VLAN" onclick="return confirm('Do you really want to delete this VLAN?')"></td>
				</tr>
			  <?php $i++; endforeach; ?>
                <tr> 
                  <td class="list" colspan="3">&nbsp;</td>
                  <td class="list"> <a href="interfaces_vlan_edit.php"><img src="plus.gif" title="add VLAN" width="17" height="17" border="0" alt="add VLAN"></a></td>
				</tr>
				<tr>
				<td colspan="3" class="list"><span class="vexpl"><span class="red"><strong>
				  Note:<br>
				  </strong></span>
				  Not all drivers/NICs support 802.1Q VLAN tagging properly. On cards that do not explicitly support it, VLAN tagging will still work, but the reduced MTU may cause problems. See the m0n0wall homepage for information on supported cards.</span>
				  </td>
				<td class="list">&nbsp;</td>
				</tr>
              </table>
			  </td>
	</tr>
</table>
</form>
<?php include("fend.inc"); ?>
