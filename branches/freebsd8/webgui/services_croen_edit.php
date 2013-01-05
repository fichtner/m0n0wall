#!/usr/local/bin/php
<?php
/*
	$Id$
	part of m0n0wall (http://m0n0.ch/wall)
	
	Copyright (C) 2012 Lennart Grahl <lennart.grahl@gmail.com>.
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

	// Sort WOL list (workaround for freeze issue)
	wol_sort();
	
	// Get job row for table in form
	function croen_get_job_row($data, &$form, &$save, &$js, &$input_errors, $pre, $pconfig = Array(), $jobC = 0, $tree = '') {
		if (isset($data['type']) && isset($data['name'])) {
			// HTML ID & NAME attribute
			$tree = ($tree != $pre && !empty($tree) ? $tree.'-' : '');

			// Type: select, Required: name, values (Array || String)
			if ($data['type'] == 'select' && isset($data['values'])) {
				// values (Array)
				if (is_array($data['values'])) {
					$d = croen_vars(Array('descr')); $d = $d['descr'];
					foreach ($data['values'] AS $k => $v) {
						// Save data: name
						if (isset($pconfig[$tree.$data['name']]) && $pconfig[$tree.$data['name']] == (!is_int($k) ? $k : $v)) {
							$save['name'] = ($tree.$data['name'] != $pre ? $tree.$data['name'].'-' : '').$pconfig[$tree.$data['name']];
						}
						$values[$k] = (isset($d[$k]) ? $d[$k] : $k);
					}
					
				// values (String) -> Target
				} else {
					$t = croen_vars(Array('target')); $t = $t['target'];
					$values = (!empty($t[$data['values']]) ? $t[$data['values']] : Array('!' => '<empty>'));
					// Save data: target
					if (isset($pconfig[$tree.$data['name']])) {
						foreach ($t[$data['values']] AS $k => $v) {
							if ($pconfig[$tree.$data['name']] == (!is_int($k) ? $k : $v)) {
								$save['target'] = $pconfig[$tree.$data['name']];
							}
						}
					}
					// Input error if <empty> is selected during save attempt
					if (empty($t[$data['values']]) && isset($pconfig[$tree.$data['name']])) {
						$d = croen_vars(Array('descr')); $d = $d['descr'];
						$fname = '';
						foreach (explode("-", $tree.$data['name']) AS $v) {
							$fname .= (isset($d[$v]) ? $d[$v] : $v).' ';
						}
						$fname = trim($fname);
						$input_errors[] = "The selected option for job '".$fname."' is invalid.";
					}
				}

				// JavaScript data
				if ($js !== FALSE) {
					$js .= ($data['name'] != $pre ? '"'.$tree.$data['name'].'", ' : '');
				}
				// Select data
				$form .= '
								<select id="'.$pre.'_'.$jobC.'_'.$tree.$data['name'].'" onChange="job_change('.$jobC.')" name="'.$pre.'['.$jobC.']['.$tree.$data['name'].']" class="formfld">';
				foreach ($values AS $k => $v) {
					$form .= '
									<option'.(!is_int($k) ? ' value="'.$k.'"' : '').(isset($pconfig[$tree.$data['name']]) && $pconfig[$tree.$data['name']] == (!is_int($k) ? $k : $v) ? ' selected' : '').'>'.htmlspecialchars($v).'</option>';
				}
				$form .= '
								</select>';

				// Not empty -> continue
				if (!isset($values['!'])) {
					// values (Array) children
					if (is_array($data['values'])) {
						foreach ($data['values'] AS $k => $v) {
							if (isset($v['name']) && isset($v['type'])) {
								croen_get_job_row($v, &$form, &$save, &$js, &$input_errors, $pre, $pconfig, $jobC, $tree.$data['name']);
							}
						}

					// values (String) children
					} elseif (isset($data['child']) && is_array($data['child'])) {
						croen_get_job_row($data['child'], &$form, &$save, &$js, &$input_errors, $pre, $pconfig, $jobC, $tree.$data['name']);
					}
				}
				
			// Type: text || digit, Required: valid, name, Optional: value, descr
			} elseif (isset($data['valid']) && ($data['type'] == 'digit' || $data['type'] == 'text')) {
				// JavaScript data
				if ($js !== FALSE) {
					$js .= ($data['name'] != $pre ? '"'.$tree.$data['name'].'", ' : '');
				}
				
				// Validate pconfig data
				if (isset($pconfig[$tree.$data['name']])) {
					$data['value'] = $pconfig[$tree.$data['name']];
					$d = croen_vars(Array('descr')); $d = $d['descr'];
					$fname = '';
					foreach (explode("-", (!empty($tree) ? $tree : $pconfig['job'])) AS $v) {
						$fname .= (isset($d[$v]) ? $d[$v] : $v).' ';
					}
					$fname = trim($fname);

					// Invalid characters
					if (preg_match("/[\\x00-\\x08\\x0b\\x0c\\x0e-\\x1f]/", $data['value'])) {
						$input_errors[] = "The field".(isset($data['descr']) ? " '".$data['descr']."'" : "")." for job '".$fname."' contains invalid characters.";
					}

					// Empty
					if ($data['value'] == '') {
						$input_errors[] = "The field".(isset($data['descr']) ? " '".$data['descr']."'" : "")." for job '".$fname."' is required.";

					// Digit
					} elseif ($data['type'] == 'digit') {
						$range = explode(",", $data['valid']);
						if (!ctype_digit($data['value'])) {
							$input_errors[] = "The field".(isset($data['descr']) ? " '".$data['descr']."'" : "")." for job '".$fname."' contains a non-digit character.";
						} elseif (($range[0] != '*' && (int)$data['value'] < (int)$range[0]) || ($range[1] != '*' && (int)$data['value'] > (int)$range[1])) {
							$input_errors[] = "The field".(isset($data['descr']) ? " '".$data['descr']."'" : "")." for job '".$fname."' has to be between ".$range[0]." and ".$range[1];
						}

					// Text
					} elseif ($data['valid'] !== FALSE) {
						if (!preg_match($data['valid'], $data['value'])) {
							$input_errors[] = "The field for job '".$fname."' contains invalid characters.";
						}
					}
					
					// Save data: value
					$save['value'] = $pconfig[$tree.$data['name']];
				}
				
				// Input data
				$form .= '
								<span id="'.$pre.'_'.$jobC.'_'.$tree.$data['name'].'"><input name="'.$pre.'['.$jobC.']['.$tree.$data['name'].']" type="text" class="formfld" size="'.
								($data['type'] == 'digit' ? '5' : '40').'" value="'.(isset($data['value']) ? htmlspecialchars($data['value']) : '').'">'.
								(isset($data['descr']) ? ' '.$data['descr'] : '').'</span>';
			}
		}
	}

	// Default config
	croen_set_default_config();
	// Shared data
	$data = croen_vars(Array('job', 'descr', 'repeat', 'date_weekly'));
	
	// Jobs data
	if (isset($_GET['id']) && isset($config['croen']['jobset'][$_GET['id']]) && isset($config['croen']['jobset'][$_GET['id']]['job']) && is_array($config['croen']['jobset'][$_GET['id']]['job'])) {
		// Edit mode
		foreach ($config['croen']['jobset'][$_GET['id']]['job'] AS $job_id => $job) {
			if (isset($job['name'])) {
				$req = croen_job_exists($job['name'], Array('req')); $req = $req['req'];
				// Check for existance of required attributes
				if (is_array($req) && $req['target'] == isset($job['target']) && $req['value'] == isset($job['value'])) {
					$name = explode("-", $job['name']);
					$jobs_data[$job_id]['job'] = $name[0];
					$tree = '';
					for ($i = 0; $i < count($name); $i++) {
						if (isset($name[$i + 1])) {
							// Name
							$jobs_data[$job_id][(!empty($tree) ? $tree.'-'.$name[$i] : $name[$i])] = $name[$i + 1];
						}
						$tree = (!empty($tree) ? $tree.'-'.$name[$i] : $name[$i]);
					}
					if (!empty($tree)) {
						if (isset($job['target'])) {
							// Target
							$jobs_data[$job_id][$tree] = $job['target'];
						}
						if (isset($job['value'])) {
							// Value
							$jobs_data[$job_id][$tree.(isset($job['target']) ? '-value' : '')] = $job['value'];
						}
					}
				}
			}
		}
	} elseif (is_array($_POST['job']) && !empty($_POST['job'])) {
		// POST jobs
		$jobs_data = $_POST['job'];
		// Add another job if button clicked
		if (isset($_POST['add_job'])) {
			$jobs_data[]['job'] = 'reconnect_wan';
		}
		// Delete a job if button clicked
		if (isset($_POST['del_job']) && is_array($_POST['del_job'])) {
			foreach (array_keys($_POST['del_job']) AS $k) {
				unset($jobs_data[$k]);
			}
		}
	}

	// If jobs data undefined -> set default
	if (!isset($jobs_data)) {
		$jobs_data[0]['job'] = 'reconnect_wan';
	}

	// Get jobs for table in form (and validate them)
	$jobs_form = Array(); $jobs_save = Array(); $jobs_js = ''; $job_change = ''; unset($input_errors);
	foreach ($jobs_data AS $job_id => $job_data) {
		$jobs_form[$job_id] = '';
		$jobs_save[$job_id] = Array();
		$tmpjs = (empty($jobs_js) ? '' : FALSE);
		croen_get_job_row($data['job'], &$jobs_form[$job_id], &$jobs_save[$job_id], &$tmpjs, &$input_errors, 'job', $job_data, $job_id);
		if (empty($jobs_js)) {
			$jobs_js = $tmpjs;
		}
		$job_change .= '
			job_change('.$job_id.');';
	}

	// Save
	if (isset($_POST['save'])) {
		// Input validation: Repeat
		if (!isset($_POST['repeat']) || !isset($data['repeat'][$_POST['repeat']])) {
			$c_input_errors[] = 'Repeat';
		}

		// Input validation: Time
		if ($_POST['repeat'] != 'x_minute') {
			$reqdfields[] = 'time'; $reqdfieldsn[] = 'Repeat';
			if (!isset($_POST['time'][0]) || !isset($_POST['time'][1]) || $_POST['time'][0] == '' || $_POST['time'][1] == '') {
				$c_input_errors[] = 'Time';
			} elseif (preg_match("/[\\x00-\\x08\\x0b\\x0c\\x0e-\\x1f]/", $_POST['time'][0]) || preg_match("/[\\x00-\\x08\\x0b\\x0c\\x0e-\\x1f]/", $_POST['time'][1])) {
				$input_errors[] = "The field 'Time' contains invalid characters.";
			} elseif (strlen($_POST['time'][0]) < 2 || strlen($_POST['time'][1]) < 2 || !ctype_digit($_POST['time'][0]) || !ctype_digit($_POST['time'][1]) || (int)$_POST['time'][0] < 0 || (int)$_POST['time'][0] > 23 || (int)$_POST['time'][1] < 0 || (int)$_POST['time'][1] > 59) {
				$input_errors[] = "The field 'Time' contains an invalid time.";
			}
		} else {
			unset($_POST['time']);
		}
		
		// Input validation: Once -> date
		if ($_POST['repeat'] == 'once') {
			unset($_POST['weekday'], $_POST['day'], $_POST['minute']);
			if (!empty($_POST['date'])) {
				if (preg_match("/[\\x00-\\x08\\x0b\\x0c\\x0e-\\x1f]/", $_POST['date'])) {
					$input_errors[] = "The field 'Date' contains invalid characters.";
				} else {
					$check_date = explode("/", $_POST['date']);
					if (!isset($check_date[0]) || !isset($check_date[1]) || !isset($check_date[2]) || !checkdate($check_date[0], $check_date[1], $check_date[2])) {
						$input_errors[] = "The field 'Date' contains an invalid date.";
					}
				}
			} else {
				$c_input_errors[] = 'Date';
			}
		}
		
		// Input validation: Weekly -> weekday
		elseif ($_POST['repeat'] == 'weekly') {
			unset($_POST['date'], $_POST['day'], $_POST['minute']);
			if (empty($_POST['weekday']) || !isset($data['date_weekly'][$_POST['weekday']])) {
				$c_input_errors[] = 'Weekday';
			}
		}
		
		// Input validation: Monthly -> day
		elseif ($_POST['repeat'] == 'monthly') {
			unset($_POST['date'], $_POST['weekday'], $_POST['minute']);
			if (!empty($_POST['day']) && ctype_digit($_POST['day']) && (int)$_POST['day'] > 0 && (int)$_POST['day'] < 29) {
				$_POST['day'] = (int)$_POST['day'];
			} else {
				if (empty($_POST['day'])) {
					$c_input_errors[] = 'Day of the month';
				} else {
					$input_errors[] = "The field 'Day of the month' contains an invalid value.";
				}
			}
		}
		
		// Input validation: Daily -> none
		elseif ($_POST['repeat'] == 'daily') {
			unset($_POST['date'], $_POST['weekday'], $_POST['day'], $_POST['minute']);
		}

		// Input validation: Every x minute -> minute
		elseif ($_POST['repeat'] == 'x_minute') {
			unset($_POST['date'], $_POST['weekday'], $_POST['day']);
			if (!empty($_POST['minute']) && ctype_digit($_POST['minute']) && (int)$_POST['minute'] > 0 && (int)$_POST['minute'] < 1440) {
				$_POST['minute'] = (int)$_POST['minute'];
			} else {
				if (empty($_POST['minute'])) {
					$c_input_errors[] = 'Interval';
				} else {
					$input_errors[] = "The field 'Interval' contains an invalid value.";
				}
			}
		}
		
		// Input validation: Invalid repeat type
		else {
			$input_errors[] = "The field 'Repeat' contains an invalid value.";
		}
		
		// Input validation: Description
		if (preg_match("/[\\x00-\\x08\\x0b\\x0c\\x0e-\\x1f]/", $_POST['descr'])) {
			$input_errors[] = "The field 'Description' contains invalid characters.";
		}
		
		// Save
		if (isset($_POST['save']) && !isset($c_input_errors) && !isset($input_errors)) {
			$save = Array();
			// Check for existance of required attributes
			foreach ($jobs_save AS $k => $job) {
				if (is_array($job) && isset($job['name'])) {
					$req = croen_job_exists($job['name'], Array('req')); $req = $req['req'];
					if (!is_array($req) || ($req['target'] && !isset($job['target'])) || ($req['value'] && !isset($job['value']))) {
						unset($jobs_save[$k]);
					} else {
						$save['job'][] = $job;
					}
				} else {
					unset($jobs_save[$k]);
				}
			}
			
			// Check if jobs are set & save
			if (isset($save['job'])) {
				// Save config
				$save['disabled'] = (isset($_POST['disabled']) ? true : false);
				$save['repeat'] = $_POST['repeat'];
				$save['syslog'] = (isset($_POST['syslog']) ? true : false);
				$save['time'] = (isset($_POST['time']) ? $_POST['time'][0].':'.$_POST['time'][1] : false);
				$save['date'] = (isset($_POST['date']) ? $_POST['date'] : false);
				$save['weekday'] = (isset($_POST['weekday']) ? $_POST['weekday'] : false);
				$save['day'] = (isset($_POST['day']) ? $_POST['day'] : false);
				$save['minute'] = (isset($_POST['minute']) ? $_POST['minute'] : false);
				$save['descr'] = $_POST['descr'];
				
				if (isset($_POST['id']) && $config['croen']['jobset'][$_POST['id']]) {
					$config['croen']['jobset'][$_POST['id']] = $save;
				} else {
					$config['croen']['jobset'][] = $save;
				}
				
				// Write config, set dirty & reroute...
				if (isset($config['croen']['enable'])) {
					touch($d_croendirty_path);
				}
				write_config();
				header("Location: services_croen.php");
				exit;
			} else {
				$c_input_errors[] = 'Job';
			}
		}
		
		// Error
		if (isset($c_input_errors)) {
			foreach ($c_input_errors AS $err) {
				$input_errors[] = "The field '".$err."' is required.";
			}
		}	
	} else {
		unset($input_errors);
	}

	// Include webinterface
	include("fbegin.inc");
	
	// Form vars
	if (isset($_GET['id']) && isset($config['croen']['jobset'][$_GET['id']])) {
		// Edit - no save attempt
		$jobset_id = $_GET['id'];
		$pconfig = $config['croen']['jobset'][$_GET['id']];
		$pconfig['time'] = explode(":", $pconfig['time']);
	} elseif (!empty($_POST)) {
		if (isset($_POST['id']) && isset($config['croen']['jobset'][$_POST['id']])) {
			// Edit - save attempt
			$jobset_id = $_POST['id'];
		}
		// Add/Edit - save attempt
		$fields = Array('job', 'disabled', 'repeat', 'syslog', 'descr', 'time', 'date', 'weekday', 'day', 'minute');
		foreach ($fields AS $v) {
			if (isset($_POST[$v])) {
				$pconfig[$v] = $_POST[$v];
			}
		}
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
			function repeat_change(repeat) {
				if (repeat == "once") {
					var show = new Array("time", "date");
				} else if (repeat == "daily") {
					var show = new Array("time");
				} else if (repeat == "weekly") {
					var show = new Array("time", "weekday");
				} else if (repeat == "monthly") {
					var show = new Array("time", "day");
				} else if (repeat == "x_minute") {
					var show = new Array("interval");
				}
				var hide = new Array("time", "date", "weekday", "day", "interval");
				for (var i = 0; i < hide.length; i++) {
					document.getElementById("repeat_" + hide[i]).style.display = "none";
				}
				for (var i = 0; i < show.length; i++) {
					document.getElementById("repeat_" + show[i]).style.display = "table-row";
				}
			}
			
			function job_change(id, job) {
				var pre = "job_" + id + "_";
				if (!job) {
					job = "job";
					var disable = new Array('.substr($jobs_js, 0, -2).');
					for (var i = 0; i < disable.length; i++) {
						if (document.getElementById(pre + disable[i])) {
							document.getElementById(pre + disable[i]).style.display = "none";
							if (document.getElementById(pre + disable[i]).tagName == "SPAN") {
								if (document.getElementById(pre + disable[i]).childNodes[0] && document.getElementById(pre + disable[i]).childNodes[0].tagName == "INPUT") {
									document.getElementById(pre + disable[i]).childNodes[0].disabled = true;
								}
							} else {
								document.getElementById(pre + disable[i]).disabled = true;
							}
						}
					}
					if (document.getElementById(pre + job) && document.getElementById(pre + job).value) {
						job_change(id, document.getElementById(pre + job).value);
					}
				} else if (document.getElementById(pre + job)) {
					if (document.getElementById(pre + job)) {
						document.getElementById(pre + job).style.display = "inline";
						if (document.getElementById(pre + job).tagName == "SPAN") {
							if (document.getElementById(pre + job).childNodes[0] && document.getElementById(pre + job).childNodes[0].tagName == "INPUT") {
								document.getElementById(pre + job).childNodes[0].disabled = false;
							}
						} else {
							document.getElementById(pre + job).disabled = false;
						}
					}					
					var element = null;
					if (document.getElementById(pre + job).value && document.getElementById(pre + job + "-" + document.getElementById(pre + job).value)) {
						element = job + "-" + document.getElementById(pre + job).value;
					} else if (document.getElementById(pre + job + "-value")) {
						element = job + "-value";
					}
					if (element) {
						job_change(id, element);
					}
				}
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
			<table width="100%" border="0" cellpadding="0" cellspacing="0" summary="tab pane">
				<tr><td class="tabcont">
					<table width="100%" border="0" cellpadding="0" cellspacing="0" summary="content pane">
						<tr>
							<td width="97%" class="listhdr">Jobs</td>
							<td width="3%" class="listhdrr">&nbsp;</td>
						</tr>';
	$last_id = -1;
	foreach ($jobs_form AS $job_id => $job) {
		if ($last_id != -1) {
			echo '
							<td class="listr">
								<input type="image" name="del_job['.$last_id.'][]" src="x.gif" title="delete job" width="17" height="17" border="0" alt="delete job">
							</td>
						</tr>';
		}
		echo '
						<tr>
							<td class="listlr" style="border-right:none;">'.$job.'</td>';
		$last_id = $job_id;
	}
	echo '
							<td class="listr" style="border-bottom:1px solid #999999;">
								<input type="image" name="add_job[]" src="plus.gif" title="add job" width="17" height="17" border="0" alt="add job">
							</td>
						</tr>
					</table>
					<table width="100%" border="0" cellpadding="6" cellspacing="0" summary="content pane">
						<tr>
							<td width="22%" valign="top" class="vncellreq">Disabled</td>
							<td width="78%" class="vtable"> 
								<input name="disabled" type="checkbox" id="disabled" value="yes"'.(isset($pconfig['disabled']) ? ' checked' : '').'>
								<strong>Disable this job</strong><br>
								<span class="vexpl">Set this option to disable this job without removing it from the list.</span>
							</td>
						</tr>
						<tr>
							<td width="22%" valign="top" class="vncellreq">Repeat</td>
							<td width="78%" class="vtable">
								<select id="croen_repeat" onChange="repeat_change(this.value)" name="repeat" class="formfld">';
	foreach ($data['repeat'] AS $repeat_name => $repeat_descr) {
		echo '
									<option value="'.$repeat_name.'"'.(isset($pconfig['repeat']) && $pconfig['repeat'] == $repeat_name ? ' selected' : '').'>'.$repeat_descr.'</option>';
	}
	echo '
								</select><br>
								<span class="vexpl">How often would you like to repeat the job?</span>
							</td>
						</tr>
						<tr>
							<td width="22%" valign="top" class="vncellreq">Log</td>
							<td width="78%" class="vtable">
								<input name="syslog" type="checkbox" value="yes"'.(isset($pconfig['syslog']) ? ' checked' : '').'>
								<strong>Make a log entry</strong><br>
								<span class="vexpl">Do you want to make a log entry if the job has been executed? The logs can be accessed here: <a href="diag_logs.php">Diagnostics: Logs</a>
							</td>
						</tr>
						<tr id="repeat_time">
							<td width="22%" valign="top" class="vncellreq">Time</td>
							<td width="78%" class="vtable">
								<input name="time[0]" type="text" class="formfld" size="2" value="'.(is_array($pconfig['time']) ? htmlspecialchars($pconfig['time'][0]) : '').'"> :
								<input name="time[1]" type="text" class="formfld" size="2" value="'.(is_array($pconfig['time']) ? htmlspecialchars($pconfig['time'][1]) : '').'"><br>
								<span class="vexpl">At what time should the job be executed?<br>
								Enter the time in the following format: hh:mm (24-hour notation)</span>
							</td>
						</tr>
						<tr id="repeat_date">
							<td width="22%" valign="top" class="vncellreq">Date</td>
							<td width="78%" class="vtable">
								<input name="date" type="text" class="formfld" id="oncedate" size="10" value="'.htmlspecialchars($pconfig['date']).'">
								<a href="javascript:NewCal(\'oncedate\',\'mmddyyyy\')"><img src="cal.gif" width="16" height="16" border="0" alt="Pick a date"></a><br>
								<span class="vexpl">At what date should the job be executed?<br>
								Enter the date in the following format: mm/dd/yyyy</span>
							</td>
						</tr>
						<tr id="repeat_weekday">
							<td width="22%" valign="top" class="vncellreq">Weekday</td>
							<td width="78%" class="vtable">
								<select name="weekday" class="formfld">';
	foreach ($data['date_weekly'] AS $weekly_nr => $weekly_descr) {
		echo '
									<option value="'.$weekly_nr.'"'.(isset($pconfig['weekday']) && $pconfig['weekday'] == $weekly_nr ? ' selected' : '').'>'.$weekly_descr.'</option>';
	}
	echo '				
								</select><br>
								<span class="vexpl">At what weekday should the job be executed?</span>
							</td>
						</tr>
						<tr id="repeat_day">
							<td width="22%" valign="top" class="vncellreq">Day of the month</td>
							<td width="78%" class="vtable">
								<input name="day" type="text" class="formfld" size="2" value="'.htmlspecialchars($pconfig['day']).'"><br>
								<span class="vexpl">At what day of the month should the job be executed? Enter a number between 1 and 28.</span>
							</tr>
						</tr>
						<tr id="repeat_interval">
							<td width="22%" valign="top" class="vncellreq">Interval</td>
							<td width="78%" class="vtable">
								Every <input name="minute" type="text" class="formfld" size="2" value="'.htmlspecialchars($pconfig['minute']).'"> minute(s)<br>
								<span class="vexpl">Enter a number between 1 and 1439</span>
							</tr>
						</tr>
						<tr> 
							<td width="22%" valign="top" class="vncell">Description</td>
							<td width="78%" class="vtable">
								<input name="descr" type="text" class="formfld" size="40" value="'.htmlspecialchars($pconfig['descr']).'"><br>
								<span class="vexpl">You may enter a description here for your reference.</span>
							</td>
						</tr>
						<tr>
							<td width="22%" valign="top">&nbsp;</td>
							<td width="78%"> 
								<input name="save" type="submit" class="formbtn" value="Save">
							</td>
						</tr>
					</table>
				</td></tr>
			</table>
			'.(isset($jobset_id) ? '<input name="id" type="hidden" value="'.htmlspecialchars($jobset_id).'">' : '').'
		</form>
		<script type="text/javascript">
			<!--
			repeat_change(document.getElementById("croen_repeat").value);'.$job_change.'
			//-->
		</script>';

	// m0n0wall
	include("fend.inc");

?>