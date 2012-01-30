#!/usr/local/bin/php
<?php
/*
	$Id: ext_croen_setup.php 1 2012-01-28 16:51:00Z masterkeule $
	extension for m0n0wall (http://m0n0.ch/wall)
	
	Copyright (C) 2012 Lennart Grahl <masterkeule@gmail.com>.
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

	// Title
	$pgtitle = array("Services", "Scheduler");

	// m0n0wall & shared functions
	require_once("guiconfig.inc");

	// Save
	if ($_POST) {
		unset($input_errors);

		// Input validation
		if ($_POST['enable']) {
			$reqdfields = Array("interval");
			$reqdfieldsn = Array("Loop interval");
			do_input_validation($_POST, $reqdfields, $reqdfieldsn, &$input_errors);
		}
		if (!$input_errors) {
			$t = (int)$_POST['interval'];
			if ($t < 1 || $t > 720) {
				$input_errors[] = "The loop interval must be between 1 and 720";
			} else {
				$config['croen']['interval'] = $t;	
				$config['croen']['enable'] = $_POST['enable'] ? true : false;
					
				write_config();
				$retval = 0;
				if (!file_exists($d_sysrebootreqd_path)) {
					config_lock();
					$retval = services_croen_configure();
					config_unlock();
				}
				$savemsg = get_std_save_message($retval);
			}
		}
	}
	
	// Shared vars
	croen_vars(&$croen);
	// Croen form vars
	$pconfig['enable'] = isset($config['croen']['enable']);
	$pconfig['interval'] = (isset($config['croen']['interval']) ? $config['croen']['interval'] : 60);
	$pconfig['job'] = &$config['croen']['job'];

	// Delete Job
	if ($_GET['act'] == 'del') {
		if ($pconfig['job'][$_GET['id']]) {
			unset($pconfig['job'][$_GET['id']]);

			// Write config & restart croen scheduler
			write_config();
			services_croen_configure();

			// Reroute
			header("Location: services_croen.php");
			exit;
		}
	}

	// Include webinterface
	include("fbegin.inc");

	// JavaScript to modify forms
	echo '
		<script type="text/javascript">
			<!--
			function enable_change(enable_change) {
				var endis;
				endis = !(document.iform.enable.checked || enable_change);
				document.iform.interval.disabled = endis;
			}
			//-->
		</script>';

	// Show errors (if any)
	if ($input_errors) {
		print_input_errors($input_errors);
	}
	// Show savemsg (if any)
	if ($savemsg) {
		print_info_box($savemsg);
	}

	// Show form
	echo '
		<form action="services_croen.php" method="post" name="iform" id="iform">
			<table width="100%" border="0" cellpadding="6" cellspacing="0" summary="content pane">
				<tr>
					<td width="22%" valign="top" class="vtable">&nbsp;</td>
					<td width="78%" class="vtable">
						<input name="enable" type="checkbox" id="enable" value="yes" onClick="enable_change(false)"'.($pconfig['enable'] ? ' checked' : '').'>
						<strong>Enable scheduler</strong>
					</td>
				</tr>
				<tr>
					<td width="22%" valign="top" class="vncell">Loop interval</td>
					<td width="78%" class="vtable">
						<input name="interval" type="text" class="formfld" id="interval" size="2" value="'.htmlspecialchars($pconfig['interval']).'"> minutes<br>
						The loop interval is used to compensate for sudden changes in time and date. An example would be the switch to daylight saving time/standard time or time correction from the NTP client.
						If your router gets continously corrected by the NTP client you should decrease the loop interval.<br>
						Default is 60 minutes.
					</td>
				</tr>
				<tr>
					<td width="22%" valign="top">&nbsp;</td>
					<td width="78%">
						<input name="Submit" type="submit" class="formbtn" value="Save" onClick="enable_change(true)"> 
					</td>
				</tr>
			</table>
		</form><br><br><br>
		<table width="100%" border="0" cellpadding="0" cellspacing="0" summary="tab pane">
			<tr>
				<td width="3%" class="list">&nbsp;</td>
				<td class="listhdrr">Job</td>
				<td class="listhdrr">Repeat</td>
				<td class="listhdrr">Date/Time</td>
				<td class="listhdr">Description</td>
				<td class="list"></td>
			</tr>';
		
		// Jobs
		foreach ($pconfig['job'] AS $croen_job_id => $croen_job) {
			echo '
			<tr>
				<td class="listt" align="center"'.(isset($croen_job['syslog']) ? '><img src="log.gif" width="11" height="11" border="0">' : '&nbsp;').'</td>
				<td class="listlr">'.$croen['descr'][$croen_job['name']][0].'</td>
				<td class="listr">'.$croen['repeat'][$croen_job['repeat']].'</td>
				<td class="listr">'.
				($croen_job['repeat'] == 'once' ? date($croen['date_once'], strtotime(htmlspecialchars($croen_job['date']).' '.htmlspecialchars($croen_job['time']))) : 
				($croen_job['repeat'] == 'daily' ? htmlspecialchars($croen_job['time']) : 
				($croen_job['repeat'] == 'weekly' ? $croen['date_weekly'][htmlspecialchars($croen_job['weekday'])].', '.htmlspecialchars($croen_job['time']) : 
				($croen_job['repeat'] == 'monthly' ? htmlspecialchars($croen_job['day']).', '.htmlspecialchars($croen_job['time']) : ''))))
				.'</td>
				<td class="listbg">'.$croen['descr'][$croen_job['name']][1].'</td>
				<td valign="middle" nowrap class="list">
					<a href="services_croen_edit.php?id='.$croen_job_id.'"><img src="e.gif" title="edit job" width="17" height="17" border="0" alt="edit job"></a>
					<a href="services_croen.php?act=del&amp;id='.$croen_job_id.'" onclick="return confirm(\'Do you really want to delete this job?\')"><img src="x.gif" title="delete job" width="17" height="17" border="0" alt="delete job"></a>
				</td>
			</tr>';
		}

echo '
			<tr> 
				<td class="list" colspan="5"></td>
				<td class="list">
					<a href="services_croen_edit.php"><img src="plus.gif" title="add job" width="17" height="17" border="0" alt="add job"></a>
				</td>
			</tr>
		</table>

		<script type="text/javascript">
			<!--
			enable_change(false);
			//-->
		</script>';

	// m0n0wall
	include("fend.inc");

?>