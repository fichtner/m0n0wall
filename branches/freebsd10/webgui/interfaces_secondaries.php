#!/usr/local/bin/php
<?php 
/*
	$Id: interfaces_secondaries.php 238 2008-01-21 18:33:33Z mkasper $
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

$pgtitle = array("Interfaces", htmlspecialchars($_GET['ifname']), "Add secondary IP address");
require("guiconfig.inc");

if (!is_array($config['secondaries']['secondary']))
	$config['secondaries']['secondary'] = array();

$a_secondaries = &$config['secondaries']['secondary'] ;

if ($_POST) {
	foreach ($_POST as $pn => $pv) {
		if (preg_match("/^del_(\d+)_x$/", $pn, $matches)) {
			$id = $matches[1];
			unset($a_secondaries[$id]);
			write_config();
			touch($d_sysrebootreqd_path);
			header("Location: interfaces_secondaries.php?if=" . urlencode($_GET['if']) ."&ifname=" . urlencode($_GET['ifname']));
			exit;
		}
	}
}

?>
<?php include("fbegin.inc"); ?>
<?php if ($input_errors) print_input_errors($input_errors); ?>
<?php if (file_exists($d_sysrebootreqd_path)) print_info_box(get_std_save_message(0)); ?>
<form action="interfaces_secondaries.php?if=<?=urlencode($_GET['if'])?>&amp;ifname=<?=urlencode($_GET['ifname'])?>" method="post" name="iform" id="iform">
<table width="100%" border="0" cellpadding="0" cellspacing="0" summary="tab pane">
  <tr><td class="tabnavtbl">
  <ul id="tabnav">
    <li class="tabinact1"><a href="<?php
		if ($_GET['if'] == "lan")
			echo "interfaces_lan.php";
		else if (preg_match("/^opt(\d)$/", $_GET['if'], $matches))
			echo "interfaces_opt.php?index=" . $matches[1];
	?>">Primary configuration</a></li>
    <li class="tabact">Secondary IPs</li>
  </ul>
  </td></tr>
  <tr> 
    <td class="tabcont">
<table width="100%" border="0" cellpadding="0" cellspacing="0" summary="content pane">
                <tr>
                  <td width="20%" class="listhdrr">IP address</td>
                  <td width="50%" class="listhdr">Description</td>
                  <td width="10%" class="list"></td>
				</tr>
			  <?php $i = 0; foreach ($a_secondaries as $secondary):
			 	if ($_GET['if'] == $secondary['if']) {?>
                <tr>
                  <td class="listlr">
					<?=htmlspecialchars($secondary['ipaddr']);?>/<?=htmlspecialchars($secondary['prefix']);?>
                  </td>
                  <td class="listbg">
                    <?=htmlspecialchars($secondary['descr']);?>&nbsp;
                  </td>
                  <td valign="middle" nowrap class="list"> <a href="interfaces_secondaries_edit.php?id=<?=$i;?>&amp;if=<?=urlencode($_GET['if'])?>&amp;ifname=<?=urlencode($_GET['ifname'])?>"><img src="e.gif" title="edit IP Address" width="17" height="17" border="0" alt="edit IP Address"></a>
                     &nbsp;<input name="del_<?=$i;?>" type="image" src="x.gif" width="17" height="17" title="delete address" alt="delete address" onclick="return confirm('Do you really want to delete this IP address?')"></td>
				</tr>
				
			  <?php } ; $i++; endforeach; ?>
                <tr> 
                  <td class="list" colspan="2">&nbsp;</td>
                  <td class="list"> <a href="interfaces_secondaries_edit.php?if=<?=urlencode($_GET['if'])?>&amp;ifname=<?=urlencode($_GET['ifname'])?>"><img src="plus.gif" title="add IP Address" width="17" height="17" border="0" alt="add IP Address"></a></td>
				</tr>
				<tr>
				<td colspan="2" class="list"><span class="vexpl"><span class="red"><strong>
				  Note:<br>
				  </strong></span>
				  Applying changes will reset the Firewall and may cause disconnection.</span>
				  </td>
				<td class="list">&nbsp;</td>
				</tr>
              </table>
			  </td>
	</tr>
</table>
</form>
<?php include("fend.inc"); ?>
