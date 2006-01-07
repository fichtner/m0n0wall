#!/usr/local/bin/php
<?php 
/*
	firewall_shaper.php
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

if (!is_array($config['shaper']['rule'])) {
	$config['shaper']['rule'] = array();
}
$a_shaper = &$config['shaper']['rule'];

$pconfig['enable'] = isset($config['shaper']['enable']);

if ($_POST) {

	if ($_POST['submit']) {
		$pconfig = $_POST;
		$config['shaper']['enable'] = $_POST['enable'] ? true : false;
		write_config();
	}
	
	if ($_POST['apply'] || $_POST['submit']) {
		$retval = 0;
		if (!file_exists($d_sysrebootreqd_path)) {
			config_lock();
			$retval = shaper_configure();
			config_unlock();
		}
		$savemsg = get_std_save_message($retval);
		if ($retval == 0) {
			if (file_exists($d_shaperconfdirty_path))
				unlink($d_shaperconfdirty_path);
		}
	}
}

if ($_GET['act'] == "del") {
	if ($a_shaper[$_GET['id']]) {
		unset($a_shaper[$_GET['id']]);
		write_config();
		touch($d_shaperconfdirty_path);
		header("Location: firewall_shaper.php");
		exit;
	}
} else if ($_GET['act'] == "down") {
	if ($a_shaper[$_GET['id']] && $a_shaper[$_GET['id']+1]) {
		$tmp = $a_shaper[$_GET['id']+1];
		$a_shaper[$_GET['id']+1] = $a_shaper[$_GET['id']];
		$a_shaper[$_GET['id']] = $tmp;
		write_config();
		touch($d_shaperconfdirty_path);
		header("Location: firewall_shaper.php");
		exit;
	}
} else if ($_GET['act'] == "up") {
	if (($_GET['id'] > 0) && $a_shaper[$_GET['id']]) {
		$tmp = $a_shaper[$_GET['id']-1];
		$a_shaper[$_GET['id']-1] = $a_shaper[$_GET['id']];
		$a_shaper[$_GET['id']] = $tmp;
		write_config();
		touch($d_shaperconfdirty_path);
		header("Location: firewall_shaper.php");
		exit;
	}
}
?>
<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01 Transitional//EN">
<html>
<head>
<title>m0n0wall webGUI - Firewall: Traffic shaper</title>
<meta http-equiv="Content-Type" content="text/html; charset=iso-8859-1">
<link href="gui.css" rel="stylesheet" type="text/css">
</head>

<body link="#0000CC" vlink="#0000CC" alink="#0000CC">
<?php include("fbegin.inc"); ?>
<p class="pgtitle">Firewall: Traffic shaper</p>
<form action="firewall_shaper.php" method="post">
<?php if ($savemsg) print_info_box(htmlspecialchars($savemsg)); ?>
<?php if (file_exists($d_shaperconfdirty_path)): ?><p>
<?php print_info_box_np("The traffic shaper configuration has been changed.<br>You must apply the changes in order for them to take effect.");?><br>
<input name="apply" type="submit" class="formbtn" id="apply" value="Apply changes"></p>
<?php endif; ?>
<table width="100%" border="0" cellpadding="0" cellspacing="0">
  <tr> 
    <td nowrap class="tabact">Rules</td>
    <td nowrap class="tabinact"><a href="firewall_shaper_pipes.php" class="tblnk">Pipes</a></td>
    <td nowrap class="tabinact"><a href="firewall_shaper_queues.php" class="tblnk">Queues</a></td>
    <td width="100%">&nbsp;</td>
  </tr>
  <tr> 
    <td colspan="4" class="tabcont">
              <table width="100%" border="0" cellpadding="6" cellspacing="0">
                <tr> 
                  <td class="vtable"><p>
                      <input name="enable" type="checkbox" id="enable" value="yes" <?php if ($pconfig['enable'] == "yes") echo "checked";?>>
                      <strong>Enable traffic shaper<br>
                      </strong></p></td>
                </tr>
                <tr> 
                  <td> <input name="submit" type="submit" class="formbtn" value="Save"> 
                  </td>
                </tr>
              </table>
              &nbsp;<br>
              <table width="100%" border="0" cellpadding="0" cellspacing="0">
                      <tr> 
                        <td width="5%" class="listhdrrns">If</td>
                        <td width="5%" class="listhdrrns">Proto</td>
                        <td width="20%" class="listhdrr">Source</td>
                        <td width="20%" class="listhdrr">Destination</td>
                        <td width="15%" class="listhdrrns">Target</td>
                        <td width="25%" class="listhdr">Description</td>
                        <td width="10%" class="list"></td>
                      </tr>
                      <?php $i = 0; foreach ($a_shaper as $shaperent): ?>
                      <tr valign="top"> 
                        <td class="listlr"> 
                          <?php
				  $iflabels = array('lan' => 'LAN', 'wan' => 'WAN', 'pptp' => 'PPTP');
				  for ($j = 1; isset($config['interfaces']['opt' . $j]); $j++)
				  	$iflabels['opt' . $j] = $config['interfaces']['opt' . $j]['descr'];
				  echo htmlspecialchars($iflabels[$shaperent['interface']]);
				  if ($shaperent['direction'])
				  	echo "<br><img src=\"{$shaperent['direction']}.gif\" width=\"11\" height=\"11\" style=\"margin-top: 5px\">";
				  ?>
                        </td>
                        <td class="listr"> 
                          <?php if (isset($shaperent['protocol'])) echo strtoupper($shaperent['protocol']); else echo "*"; ?>
                        </td>
                        <td class="listr"> <?php echo htmlspecialchars(pprint_address($shaperent['source'])); ?>
						<?php if ($shaperent['source']['port']): ?><br>
						Port: <?=htmlspecialchars(pprint_port($shaperent['source']['port'])); ?> 
						<?php endif; ?>
                        </td>
                        <td class="listr"> <?php echo htmlspecialchars(pprint_address($shaperent['destination'])); ?>
						<?php if ($shaperent['destination']['port']): ?><br>
						Port: <?=htmlspecialchars(pprint_port($shaperent['destination']['port'])); ?>
						<?php endif; ?>
                        </td>
                        <td class="listr"> 
                          <?php 
						if (isset($shaperent['targetpipe']))
							echo "<a href=\"firewall_shaper_pipes_edit.php?id={$shaperent['targetpipe']}\">Pipe " . 
								($shaperent['targetpipe']+1) . "</a>";
						else if (isset($shaperent['targetqueue']))
							echo "<a href=\"firewall_shaper_queues_edit.php?id={$shaperent['targetqueue']}\">Queue " . 
								($shaperent['targetqueue']+1) . "</a>";
					?>
                        </td>
                        <td class="listbg"> 
                          <?=htmlspecialchars($shaperent['descr']);?>
                          &nbsp; </td>
                        <td valign="middle" nowrap class="list"> <a href="firewall_shaper_edit.php?id=<?=$i;?>"><img src="e.gif" alt="edit rule" width="17" height="17" border="0"></a> 
                          <?php if ($i > 0): ?>
                          <a href="firewall_shaper.php?act=up&id=<?=$i;?>"><img src="up.gif" alt="move up" width="17" height="17" border="0"></a> 
                          <?php else: ?>
                          <img src="up_d.gif" width="17" height="17" border="0"> 
                          <?php endif; ?><br>
						  <a href="firewall_shaper.php?act=del&id=<?=$i;?>" onclick="return confirm('Do you really want to delete this rule?')"><img src="x.gif" alt="delete rule" width="17" height="17" border="0"></a> 
                          <?php if (isset($a_shaper[$i+1])): ?>
                          <a href="firewall_shaper.php?act=down&id=<?=$i;?>"><img src="down.gif" alt="move down" width="17" height="17" border="0"></a> 
                          <?php else: ?>
                          <img src="down_d.gif" width="17" height="17" border="0"> 
                          <?php endif; ?>
                          <a href="firewall_shaper_edit.php?dup=<?=$i;?>"><img src="plus.gif" alt="add a new rule based on this one" width="17" height="17" border="0"></a> 
                        </td>
                      </tr>
                      <?php $i++; endforeach; ?>
                      <tr> 
                        <td class="list" colspan="6"></td>
                        <td class="list"> <a href="firewall_shaper_edit.php"><img src="plus.gif" width="17" height="17" border="0"></a></td>
                      </tr>
                    </table>
					  
                    <table border="0" cellspacing="0" cellpadding="0">
                      <tr> 
                        <td width="16"><img src="in.gif" width="11" height="11"></td>
                        <td>incoming (as seen by firewall)</td>
                      </tr>
                      <tr> 
                        <td colspan="5" height="4"></td>
                      </tr>
                      <tr> 
                        <td><img src="out.gif" width="11" height="11"></td>
                        <td>outgoing (as seen by firewall)</td>
                      </tr>
                    </table>
			        <p><span class="red"><strong>Note:</strong></span><strong><br>
                    </strong>the first rule that matches a packet will be executed.<br>
                    The following match patterns are not shown in the list above: 
                    IP packet length, TCP flags.</td></p>
	</tr>
</table>
            </form>
<?php include("fend.inc"); ?>
</body>
</html>
