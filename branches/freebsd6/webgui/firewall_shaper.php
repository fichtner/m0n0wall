#!/usr/local/bin/php
<?php 
/*
	$Id$
	part of m0n0wall (http://m0n0.ch/wall)
	
	Copyright (C) 2003-2012 Manuel Kasper <mk@neon1.net>.
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

$pgtitle = array("Firewall", "Traffic shaper", "Rules");
require("guiconfig.inc");

if (!is_array($config['shaper']['rule'])) {
	$config['shaper']['rule'] = array();
}
if (!is_array($config['shaper']['pipe'])) {
	$config['shaper']['pipe'] = array();
}
if (!is_array($config['shaper']['queue'])) {
	$config['shaper']['queue'] = array();
}
$a_shaper = &$config['shaper']['rule'];
$a_pipe = &$config['shaper']['pipe'];
$a_queue = &$config['shaper']['queue'];

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
	
	if (isset($_POST['del_x'])) {
		/* delete selected rules */
		if (is_array($_POST['rule']) && count($_POST['rule'])) {
			foreach ($_POST['rule'] as $rulei) {
				unset($a_shaper[$rulei]);
			}
			write_config();
			touch($d_shaperconfdirty_path);
			header("Location: firewall_shaper.php");
			exit;
		}
	} else {
		/* yuck - IE won't send value attributes for image buttons, while Mozilla does - 
		   so we use .x/.y to find move and toggle button clicks instead... */
		unset($movebtn);
		unset($togglebtn);
		foreach ($_POST as $pn => $pd) {
			if (preg_match("/move_(\d+)_x/", $pn, $matches)) {
				$movebtn = $matches[1];
				break;
			} else if (preg_match("/toggle_(\d+)_x/", $pn, $matches)) {
				$togglebtn = $matches[1];
				break;
			}
		}
		/* move selected rules before this rule */
		if (isset($movebtn) && is_array($_POST['rule']) && count($_POST['rule'])) {
			$a_shaper_new = array();

			/* copy all rules < $movebtn and not selected */
			for ($i = 0; $i < $movebtn; $i++) {
				if (!in_array($i, $_POST['rule']))
					$a_shaper_new[] = $a_shaper[$i];
			}

			/* copy all selected rules */
			for ($i = 0; $i < count($a_shaper); $i++) {
				if ($i == $movebtn)
					continue;
				if (in_array($i, $_POST['rule']))
					$a_shaper_new[] = $a_shaper[$i];
			}

			/* copy $movebtn rule */
			if ($movebtn < count($a_shaper))
				$a_shaper_new[] = $a_shaper[$movebtn];

			/* copy all rules > $movebtn and not selected */
			for ($i = $movebtn+1; $i < count($a_shaper); $i++) {
				if (!in_array($i, $_POST['rule']))
					$a_shaper_new[] = $a_shaper[$i];
			}

			$a_shaper = $a_shaper_new;
			write_config();
			touch($d_shaperconfdirty_path);
			header("Location: firewall_shaper.php");
			exit;
		}

		if (isset($togglebtn)) {
			if ($a_shaper[$togglebtn]) {
				$a_shaper[$togglebtn]['disabled'] = !isset($a_shaper[$togglebtn]['disabled']);
				write_config();
				touch($d_shaperconfdirty_path);
				header("Location: firewall_shaper.php");
				exit;
			}
		}
	}
}
?>
<?php include("fbegin.inc"); ?>
<script type="text/javascript">
<!--
function fr_toggle(id) {
	var checkbox = document.getElementById('frc' + id);
	checkbox.checked = !checkbox.checked;
	fr_bgcolor(id);
}
function fr_bgcolor(id) {
	var row = document.getElementById('fr' + id);
	var checkbox = document.getElementById('frc' + id);
	var cells = row.getElementsByTagName("td");
	
	for (i = 1; i <= 5; i++) {
		cells[i].style.backgroundColor = checkbox.checked ? "#FFFFBB" : "#FFFFFF";
	}
	cells[6].style.backgroundColor = checkbox.checked ? "#FFFFBB" : "#D9DEE8";
}
function fr_insline(id, on) {
	var row = document.getElementById('fr' + id);
	var prevrow;
	if (id != 0) {
		prevrow = document.getElementById('fr' + (id-1));
	} else {
		prevrow = document.getElementById('frheader');
	}
	
	var cells = row.getElementsByTagName("td");
	var prevcells = prevrow.getElementsByTagName("td");
	
	for (i = 1; i <= 6; i++) {
		if (on) {
			prevcells[i].style.borderBottom = "3px solid #999999";
			prevcells[i].style.paddingBottom = (id != 0) ? 2 : 3;
		} else {
			prevcells[i].style.borderBottomWidth = "1px";
			prevcells[i].style.paddingBottom = (id != 0) ? 4 : 5;
		}
	}
	
	for (i = 1; i <= 6; i++) {
		if (on) {
			cells[i].style.borderTop = "2px solid #999999";
			cells[i].style.paddingTop = 2;
		} else {
			cells[i].style.borderTopWidth = 0;
			cells[i].style.paddingTop = 4;
		}
	}
}
// -->
</script>
<form action="firewall_shaper.php" method="post">
<?php if ($savemsg) print_info_box($savemsg); ?>
<?php if (file_exists($d_shaperconfdirty_path)): ?><p>
<?php print_info_box_np("The traffic shaper configuration has been changed.<br>You must apply the changes in order for them to take effect.");?><br>
<input name="apply" type="submit" class="formbtn" id="apply" value="Apply changes"></p>
<?php endif; ?>
<table width="100%" border="0" cellpadding="0" cellspacing="0" summary="tab pane">
  <tr><td class="tabnavtbl">
  <ul id="tabnav">
<?php 
   	$tabs = array('Rules' => 'firewall_shaper.php',
           		  'Pipes' => 'firewall_shaper_pipes.php',
           		  'Queues' => 'firewall_shaper_queues.php',
           		  'Magic shaper wizard' => 'firewall_shaper_magic.php');
	dynamic_tab_menu($tabs);
?>       
  </ul>
  </td></tr>
  <tr> 
    <td class="tabcont">
              <table width="100%" border="0" cellpadding="6" cellspacing="0" summary="checkbox pane">
                <tr> 
                  <td class="vtable">
                      <input name="enable" type="checkbox" id="enable" value="yes" <?php if ($pconfig['enable']) echo "checked";?>>
                      <strong>Enable traffic shaper</strong></td>
                </tr>
                <tr> 
                  <td> <input name="submit" type="submit" class="formbtn" value="Save"> 
                  </td>
                </tr>
              </table>
              &nbsp;<br>
              <table width="100%" border="0" cellpadding="0" cellspacing="0" summary="content pane">
                      <tr id="frheader"> 
                  		<td width="5%" class="list">&nbsp;</td>
                        <td width="5%" class="listhdrrns">If</td>
                        <td width="5%" class="listhdrrns">Proto</td>
                        <td width="18%" class="listhdrr">Source</td>
                        <td width="18%" class="listhdrr">Destination</td>
                        <td width="14%" class="listhdrrns">Target</td>
                        <td width="25%" class="listhdr">Description</td>
                        <td width="10%" class="list"></td>
                      </tr>
                      <?php $i = 0; foreach ($a_shaper as $shaperent): ?>
                      <tr valign="top" id="fr<?=$i;?>"> 
                  		<td class="listt"><input type="checkbox" id="frc<?=$i;?>" name="rule[]" value="<?=$i;?>" onClick="fr_bgcolor('<?=$i;?>')" style="margin: 0; padding: 0; width: 15px; height: 15px;"></td>
                        <td class="listlr" onClick="fr_toggle(<?=$i;?>)"> 
                          <?php
				  $dis = "";
				  if (isset($shaperent['disabled'])) {
				  	$dis = "_d";
					$textss = "<span class=\"gray\">";
					$textse = "</span>";
				  } else {
				  	$textss = $textse = "";
				  }
				  $iflabels = array('lan' => 'LAN', 'wan' => 'WAN', 'pptp' => 'PPTP');
				  for ($j = 1; isset($config['interfaces']['opt' . $j]); $j++)
				  	$iflabels['opt' . $j] = $config['interfaces']['opt' . $j]['descr'];
				  echo $textss . htmlspecialchars($iflabels[$shaperent['interface']]);
				  echo "<br>";
				  if ($shaperent['direction'] != "in")
					$iconfn = "out{$dis}.gif";
				  else
				    $iconfn = "in{$dis}.gif";
				  echo "<input name=\"toggle_$i\" type=\"image\" src=\"$iconfn\" width=\"11\" height=\"11\" style=\"margin-top: 5px\" title=\"click to toggle enabled/disabled status\">";
				  echo $textse;
				  ?>
                        </td>
                        <td class="listr" onClick="fr_toggle(<?=$i;?>)">
                          <?=$textss;?><?php if (isset($shaperent['protocol'])) echo strtoupper($shaperent['protocol']); else echo "*"; ?><?=$textse;?>
                        </td>
                        <td class="listr" onClick="fr_toggle(<?=$i;?>)"><?=$textss;?><?php echo htmlspecialchars(pprint_address($shaperent['source'])); ?>
						<?php if ($shaperent['source']['port']): ?><br>
						Port: <?=htmlspecialchars(pprint_port($shaperent['source']['port'])); ?> 
						<?php endif; ?><?=$textse;?>
                        </td>
                        <td class="listr" onClick="fr_toggle(<?=$i;?>)"><?=$textss;?><?php echo htmlspecialchars(pprint_address($shaperent['destination'])); ?>
						<?php if ($shaperent['destination']['port']): ?><br>
						Port: <?=htmlspecialchars(pprint_port($shaperent['destination']['port'])); ?>
						<?php endif; ?><?=$textse;?>
                        </td>
                        <td class="listr" onClick="fr_toggle(<?=$i;?>)"><?=$textss;?>
                          <?php 
							if (isset($shaperent['targetpipe'])) {
								if ($a_pipe[$shaperent['targetpipe']]['descr'])
									$desc = htmlspecialchars($a_pipe[$shaperent['targetpipe']]['descr']);
								else 
									$desc = "Pipe " . ($shaperent['targetpipe']+1);
								echo "<a href=\"firewall_shaper_pipes_edit.php?id={$shaperent['targetpipe']}\">{$desc}</a>";
							} else if (isset($shaperent['targetqueue'])) {
								if ($a_queue[$shaperent['targetqueue']]['descr'])
									$desc = htmlspecialchars($a_queue[$shaperent['targetqueue']]['descr']);
								else 
									$desc = "Queue " . ($shaperent['targetqueue']+1);
								echo "<a href=\"firewall_shaper_queues_edit.php?id={$shaperent['targetqueue']}\">{$desc}</a>";
							}
						  ?><?=$textse;?>
                        </td>
                        <td class="listbg" onClick="fr_toggle(<?=$i;?>)">
                          <?=$textss;?><?=htmlspecialchars($shaperent['descr']);?><?=$textse;?>
                          &nbsp; </td>
                        <td valign="middle" nowrap class="list">
						    <table border="0" cellspacing="0" cellpadding="1" summary="button pane">
							<tr>
							  <td><input name="move_<?=$i;?>" type="image" src="left.gif" width="17" height="17" title="move selected rules before this rule" onMouseOver="fr_insline(<?=$i;?>, true)" onMouseOut="fr_insline(<?=$i;?>, false)"></td>
							  <td><a href="firewall_shaper_edit.php?id=<?=$i;?><?=$typelink;?>"><img src="e.gif" title="edit rule" width="17" height="17" border="0" alt="edit rule"></a></td>
							</tr>
							<tr>
							  <td align="center" valign="middle"></td>
							  <td><a href="firewall_shaper_edit.php?dup=<?=$i;?><?=$typelink;?>"><img src="plus.gif" title="add a new rule based on this one" width="17" height="17" border="0" alt="add a new rule based on this one"></a></td>
							</tr>
							</table>
                        </td>
                      </tr>
                      <?php $i++; endforeach; ?>
                      <tr> 
                        <td class="list" colspan="7"></td>
                        <td class="list">
							<?php if ($i == 0): ?><img src="x_d.gif" width="17" height="17" title="delete selected rules" border="0" alt="delete selected rules"><?php else: ?><input name="del" type="image" src="x.gif" width="17" height="17" title="delete selected rules" alt="delete selected rules" onclick="return confirm('Do you really want to delete the selected rules?')"><?php endif; ?>
							<a href="firewall_shaper_edit.php"><img src="plus.gif" title="add rule" width="17" height="17" border="0" alt="add rule"></a></td>
                      </tr>
                    </table>
					  
                    <table border="0" cellspacing="0" cellpadding="0" summary="info pane">
                      <tr> 
                        <td width="16"><img src="in.gif" width="11" height="11" alt=""></td>
                        <td>incoming (as seen by firewall)</td>
                        <td width="14"></td>
                        <td width="16"><img src="out.gif" width="11" height="11" alt=""></td>
                        <td>outgoing (as seen by firewall)</td>
                      </tr>
                      <tr> 
                        <td colspan="5" height="4"></td>
                      </tr>
                      <tr> 
                        <td><img src="in_d.gif" width="11" height="11" alt=""></td>
                        <td>incoming (disabled)</td>
                        <td width="14"></td>
                        <td><img src="out_d.gif" width="11" height="11" alt=""></td>
                        <td>outgoing (disabled)</td>
                      </tr>
                    </table><br>
			        <span class="red"><strong>Note:</strong></span><strong><br>
                    </strong>the first rule that matches a packet will be executed.<br>
                    The following match patterns are not shown in the list above: 
                    IP packet length, TCP flags.</td>
	</tr>
</table>
            </form>
<?php include("fend.inc"); ?>
