#!/usr/local/bin/php
<?php
/*
	$Id$
	part of m0n0wall (http://m0n0.ch/wall)
	
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
	if (isset($_GET['id'])) {
		$pgtitle = array("Services", "Scheduler", "Edit job");
	} else {
		$pgtitle = array("Services", "Scheduler", "Add job");
	}

	// m0n0wall & shared functions
	require_once("guiconfig.inc");

	// Shared vars
	croen_vars(&$croen);

	// Save
	if ($_POST) {
		unset($input_errors);
		
		// Input validation
		if (!empty($_POST['croen']['name']) && !empty($_POST['croen']['repeat'])) {
			// Name
			if (!isset($croen['descr'][$_POST['croen']['name']])) { $croen_input_errors[] = 'Job'; }
			// Repeat
			if (!isset($croen['repeat'][$_POST['croen']['repeat']])) { $croen_input_errors[] = 'Repeat'; }
			
			// Time
			if (!isset($_POST['croen']['time'][0]) || 
				!isset($_POST['croen']['time'][1]) || 
				$_POST['croen']['time'][0] == '' || 
				$_POST['croen']['time'][1] == '')
			{
				$croen_input_errors[] = 'Time';
			} elseif (strlen($_POST['croen']['time'][0]) < 2 || strlen($_POST['croen']['time'][1]) < 2 ||
					  !ctype_digit($_POST['croen']['time'][0]) || !ctype_digit($_POST['croen']['time'][1]) ||
					  (int)$_POST['croen']['time'][0] < 0 || (int)$_POST['croen']['time'][0] > 23 ||
					  (int)$_POST['croen']['time'][1] < 0 || (int)$_POST['croen']['time'][1] > 59)
			{
				$input_errors[] = "The field 'Time' contains an invalid time.";
			}
			
			// Once -> date
			if ($_POST['croen']['repeat'] == 'once') {
				unset($_POST['croen']['weekday'], $_POST['croen']['day']);
				if (!empty($_POST['croen']['date'])) {
					$croen_check_date = explode("/", $_POST['croen']['date']);
					if (!isset($croen_check_date[0]) || !isset($croen_check_date[1]) || !isset($croen_check_date[2]) || !checkdate($croen_check_date[0], $croen_check_date[1], $croen_check_date[2])) {
						$input_errors[] = "The field 'Date' contains an invalid date.";
					}
				} else { $croen_input_errors[] = 'Date'; }
			}
			
			// Weekly -> weekday
			elseif ($_POST['croen']['repeat'] == 'weekly') {
				unset($_POST['croen']['date'], $_POST['croen']['day']);
				if (empty($_POST['croen']['weekday']) || !isset($croen['date_weekly'][$_POST['croen']['weekday']])) {
					$croen_input_errors[] = 'Weekday';
				}
			}
			
			// Monthly -> day
			elseif ($_POST['croen']['repeat'] == 'monthly') {
				unset($_POST['croen']['date'], $_POST['croen']['weekday']);
				if (!empty($_POST['croen']['day']) && ctype_digit($_POST['croen']['day']) && (int)$_POST['croen']['day'] > 0 && (int)$_POST['croen']['day'] < 29) {
					$_POST['croen']['day'] = (int)$_POST['croen']['day'];
				} else {
					if (empty($_POST['croen']['day'])) {
						$croen_input_errors[] = 'Day of the month';
					} else {
						$input_errors[] = "The field 'Day of the month' contains an invalid value.";
					}
				}
			}
			
			// Daily -> none
			elseif ($_POST['croen']['repeat'] == 'daily') {
				unset($_POST['croen']['date'], $_POST['croen']['weekday'], $_POST['croen']['day']);
			}
			
			// invalid repeat -> error
			else {
				$input_errors[] = "Something went wrong. Please try again.";
			}
			
			if (!isset($croen_input_errors) && !isset($input_errors)) {
				$croen_save = Array();
				// Save config
				$croen_save['name'] = $_POST['croen']['name'];
				$croen_save['repeat'] = $_POST['croen']['repeat'];
				$croen_save['time'] = $_POST['croen']['time'][0].':'.$_POST['croen']['time'][1];
				$croen_save['date'] = (isset($_POST['croen']['date']) ? $_POST['croen']['date'] : false);
				$croen_save['weekday'] = (isset($_POST['croen']['weekday']) ? $_POST['croen']['weekday'] : false);
				$croen_save['day'] = (isset($_POST['croen']['day']) ? $_POST['croen']['day'] : false);
				$croen_save['syslog'] = (isset($_POST['croen']['syslog']) ? true : false);
				
				if (isset($_POST['croen']['id']) && $config['croen']['job'][$_POST['croen']['id']]) {
					$config['croen']['job'][$_POST['croen']['id']] = $croen_save;
				} else {
					$config['croen']['job'][] = $croen_save;
				}
				
				// Write config & restart croen scheduler
				write_config();
				services_croen_configure();

				// Reroute...
				header("Location: services_croen.php");
				exit;
			}
		} else {
			if (empty($_POST['croen']['name'])) { $croen_input_errors[] = 'Job'; }
			if (empty($_POST['croen']['repeat'])) { $croen_input_errors[] = 'Repeat'; }
		}
		
		// Error
		if (isset($croen_input_errors)) {
			foreach ($croen_input_errors AS $croen_err) {
				$input_errors[] = "The field '".$croen_err."' is required.";
			}
		}
	}

	// Include webinterface
	include("fbegin.inc");

	// Croen form vars
	if (isset($_GET['id']) && $config['croen']['job'][$_GET['id']]) {
		// Edit - no save attempt
		$croen_id = $_GET['id'];
		$pconfig = $config['croen']['job'][$_GET['id']];
		$pconfig['time'] = explode(":", $pconfig['time']);
	} elseif (isset($_POST['croen'])) {
		if (isset($_POST['croen']['id']) && $config['croen']['job'][$_POST['croen']['id']]) {
			// Edit - save attempt
			$croen_id = $_POST['croen']['id'];
		}
		// Add/Edit - save attempt
		$pconfig = $_POST['croen'];
	}

	// JavaScript to modify forms
	echo '
		<script language="javascript" type="text/javascript" src="datetimepicker.js">
			<!--
			//Date Time Picker script- by TengYong Ng of http://www.rainforestnet.com
			//Script featured on JavaScript Kit (http://www.javascriptkit.com)
			//For this script, visit http://www.javascriptkit.com
			// -->
		</script>
		<script type="text/javascript">
			<!--
			function enable_change(enable_change) {
				document.getElementById("repeat_date").style.display = "none";
				document.getElementById("repeat_weekday").style.display = "none";
				document.getElementById("repeat_day").style.display = "none";
				if (enable_change == "once") {
					document.getElementById("repeat_date").style.display = "table-row";
				} else if (enable_change == "weekly") {
					document.getElementById("repeat_weekday").style.display = "table-row";
				} else if (enable_change == "monthly") {
					document.getElementById("repeat_day").style.display = "table-row";
				}
			}
			
			function descr_change(job) {';
	foreach ($croen['descr'] AS $croen_job_name => $croen_job_descr) {
		echo '
				document.getElementById("descr_'.$croen_job_name.'").style.display = "none";';
	}
	echo '
				document.getElementById("descr_" + job).style.display = "block";
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
		<form action="services_croen_edit.php" method="post" name="iform" id="iform">
			<table width="100%" border="0" cellpadding="6" cellspacing="0" summary="content pane">
				<tr>
					<td width="22%" valign="top" class="vncellreq">Job</td>
					<td width="78%" class="vtable">
						<select id="croen_name" onChange="descr_change(this.value)" name="croen[name]" class="formfld">';
	foreach ($croen['descr'] AS $croen_job_name => $croen_job_descr) {
		echo '
							<option value="'.$croen_job_name.'"'.(isset($pconfig['name']) && $pconfig['name'] == $croen_job_name ? ' selected' : '').'>'.$croen_job_descr[0].'</option>';
	}
	echo '				</select><br>';
	foreach ($croen['descr'] AS $croen_job_name => $croen_job_descr) {
		echo '
						<span class="vexpl" id="descr_'.$croen_job_name.'" style="display:none">'.$croen_job_descr[1].'</span>';
	}
		echo '		</td>
				</tr>
				<tr>
					<td width="22%" valign="top" class="vncellreq">Repeat</td>
					<td width="78%" class="vtable">
						<select id="croen_repeat" onChange="enable_change(this.value)" name="croen[repeat]" class="formfld">';
	foreach ($croen['repeat'] AS $croen_repeat_name => $croen_repeat_descr) {
		echo '
							<option value="'.$croen_repeat_name.'"'.(isset($pconfig['repeat']) && $pconfig['repeat'] == $croen_repeat_name ? ' selected' : '').'>'.$croen_repeat_descr.'</option>';
	}
	echo '				</select><br>
						<span class="vexpl">How often would you like to repeat the job?</span>
					</td>
				</tr>
				<tr>
					<td width="22%" valign="top" class="vncellreq">Log</td>
					<td width="78%" class="vtable">
						<input name="croen[syslog]" type="checkbox" value="yes"'.(isset($pconfig['syslog']) ? ' checked' : '').'>
						<strong>Make log entry</strong><br>
						<span class="vexpl">Do you want to make a log entry if the job has been executed? The logs can be accessed here: <a href="diag_logs.php">Diagnostics: Logs</a>
					</td>
				</tr>
				<tr>
					<td width="22%" valign="top" class="vncellreq">Time</td>
					<td width="78%" class="vtable">
						<input name="croen[time][0]" type="text" class="formfld" size="2" value="'.(is_array($pconfig['time']) ? htmlspecialchars($pconfig['time'][0]) : '').'"> :
						<input name="croen[time][1]" type="text" class="formfld" size="2" value="'.(is_array($pconfig['time']) ? htmlspecialchars($pconfig['time'][1]) : '').'"><br>
						<span class="vexpl">At what time should the job be executed?<br>
						Enter the time in the following format: hh:mm</span>
					</td>
				</tr>
				<tr id="repeat_date">
					<td width="22%" valign="top" class="vncellreq">Date</td>
					<td width="78%" class="vtable">
						<input name="croen[date]" type="text" class="formfld" id="oncedate" size="10" value="'.htmlspecialchars($pconfig['date']).'">
						<a href="javascript:NewCal(\'oncedate\',\'mmddyyyy\')"><img src="cal.gif" width="16" height="16" border="0" alt="Pick a date"></a><br>
						<span class="vexpl">At what date should the job be executed?<br>
						Enter the date in the following format: mm/dd/yyyy</span>
					</td>
				</tr>
				<tr id="repeat_weekday">
					<td width="22%" valign="top" class="vncellreq">Weekday</td>
					<td width="78%" class="vtable">
						<select name="croen[weekday]" class="formfld">';
	foreach ($croen['date_weekly'] AS $croen_weekly_nr => $croen_weekly_descr) {
		echo '
							<option value="'.$croen_weekly_nr.'"'.(isset($pconfig['weekday']) && $pconfig['weekday'] == $croen_weekly_nr ? ' selected' : '').'>'.$croen_weekly_descr.'</option>';
	}
	echo '				</select><br>
						<span class="vexpl">At what weekday should the job be executed?</span>
					</td>
				</tr>
				<tr id="repeat_day">
					<td width="22%" valign="top" class="vncellreq">Day of the month</td>
					<td width="78%" class="vtable">
						<input name="croen[day]" type="text" class="formfld" size="2" value="'.htmlspecialchars($pconfig['day']).'"><br>
						<span class="vexpl">At what day of the month should the job be executed? Enter a number between 1 and 28.</span>
					</tr>
				</tr>
				<tr>
					<td width="22%" valign="top">&nbsp;</td>
					<td width="78%"> 
						<input name="Submit" type="submit" class="formbtn" value="Save">
					</td>
				</tr>
			</table>'.(isset($croen_id) ? "\n".'<input name="croen[id]" type="hidden" value="'.htmlspecialchars($croen_id).'">' : '').'
		</form>

		<script type="text/javascript">
			<!--
			descr_change(document.getElementById("croen_name").value);
			enable_change(document.getElementById("croen_repeat").value);
			//-->
		</script>';

	// m0n0wall
	include("fend.inc");

?>