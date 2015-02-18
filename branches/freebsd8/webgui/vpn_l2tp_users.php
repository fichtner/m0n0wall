#!/usr/local/bin/php
<?php
/*
	$Id: vpn_l2tp_users.php 522 2012-10-22 15:47:41Z mkasper $
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

$pgtitle = array("VPN", "IPsec/L2TP", "Users");
require("guiconfig.inc");

if (!is_array($config['l2tp']['user'])) {
	$config['l2tp']['user'] = array();
}
l2tp_users_sort();
$a_secret = &$config['l2tp']['user'];

if ($_POST) {

	$pconfig = $_POST;

	if ($_POST['apply']) {
		$retval = 0;
		if (!file_exists($d_sysrebootreqd_path)) {
			config_lock();
			$retval = vpn_ipsec_l2tp_configure();
			config_unlock();
		}
		$savemsg = get_std_save_message($retval);
		if ($retval == 0) {
			if (file_exists($d_l2tpuserdirty_path))
				unlink($d_l2tpuserdirty_path);
		}
	} else {
		foreach ($_POST as $pn => $pv) {
			if (preg_match("/^del_(\d+)_x$/", $pn, $matches)) {
				$id = $matches[1];
				if ($a_secret[$id]) {
					unset($a_secret[$id]);
					write_config();
					touch($d_l2tpuserdirty_path);
					header("Location: l2tp_users.php");
					exit;
				}
			}
		}
	}
}

?>
<?php include("fbegin.inc"); ?>
<form action="vpn_l2tp_users.php" method="post">
<?php if ($savemsg) print_info_box($savemsg); ?>
<?php if (isset($config['l2tp']['radius']['enable']))
	print_info_box("Warning: RADIUS is enabled. The local user database will not be used."); ?>
<?php if (file_exists($d_l2tpuserdirty_path)): ?><p>
<?php print_info_box_np("The IPsec/L2TP user list has been modified.<br>You must apply the changes in order for them to take effect.<br><b>Warning: this will terminate all current L2TP/IPsec sessions!</b>");?><br>
<input name="apply" type="submit" class="formbtn" id="apply" value="Apply changes"></p>
<?php endif; ?>
<table width="100%" border="0" cellpadding="0" cellspacing="0" summary="tab pane">
  <tr><td class="tabnavtbl">
  <ul id="tabnav">
<?php 
   	$tabs = array('Configuration' => 'vpn_ipsec_l2tp.php',
           		  'Users' => 'vpn_l2tp_users.php');
	dynamic_tab_menu($tabs);
?>
  </ul>
  </td></tr>
  <tr> 
    <td colspan="3" class="tabcont">
              <table width="80%" border="0" cellpadding="0" cellspacing="0" summary="content pane">
                <tr> 
                  <td class="listhdrr">Username</td>
                  <td class="listhdr">IP address</td>
                  <td class="list"></td>
				</tr>
			  <?php $i = 0; foreach ($a_secret as $secretent): ?>
                <tr> 
                  <td class="listlr">
                    <?=htmlspecialchars($secretent['name']);?>
                  </td>
                  <td class="listr">
                    <?=htmlspecialchars($secretent['ip']);?>&nbsp;
                  </td>
                  <td class="list" nowrap> <a href="vpn_l2tp_users_edit.php?id=<?=$i;?>"><img src="e.gif" title="edit user" width="17" height="17" border="0" alt="edit user"></a>
                     &nbsp;<input name="del_<?=$i;?>" type="image" src="x.gif" width="17" height="17" title="delete user" alt="delete user" onclick="return confirm('Do you really want to delete this user?')"></td>
				</tr>
			  <?php $i++; endforeach; ?>
                <tr> 
                  <td class="list" colspan="2"></td>
                  <td class="list"> <a href="vpn_l2tp_users_edit.php"><img src="plus.gif" title="add user" width="17" height="17" border="0" alt="add user"></a></td>
				</tr>
              </table>
			</td>
	</tr>
</table>
</form>
<?php include("fend.inc"); ?>
