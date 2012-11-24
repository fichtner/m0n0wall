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

	// Set default (croen->jobset: Array)
	function croen_set_default_config() {
		global $config;

		// Superficial corruption check
		if (is_array($config['croen']) && is_array($config['croen']['jobset'])) {
			$config_changed = FALSE;
			
			// Note: "<jobset/>" will be parsed to array([0] => '')
			if (isset($config['croen']['jobset'][0]) && empty($config['croen']['jobset'][0])) {
				$config['croen']['jobset'] = array();
				$config_changed = TRUE;
			}
			
			foreach ($config['croen']['jobset'] AS $k => $jobset) {
				// Exists and not empty: Repeat, Job
				// Note: "<job/>" will be parsed to array([0] => '') so we check for that, too
				if (
					!isset($jobset['repeat']) || empty($jobset['repeat']) || !isset($jobset['job']) ||
					(is_array($jobset['job']) && (empty($jobset['job']) || (isset($jobset['job'][0]) && empty($jobset['job'][0]))))
				) {
					unset($config['croen']['jobset'][$k]);
					$config_changed = TRUE;
				}
			}
			
			// Changed someting: Write config
			if ($config_changed) {
				write_config();
			}
		}

		// Prepare scheduler arrays
		if (!is_array($config['croen'])) {
			$config['croen'] = array();
		}
		if (!is_array($config['croen']['jobset'])) {
			$config['croen']['jobset'] = array();
		}
	}

	// Return requested vars
	function croen_vars($which = Array()) {
		// Date conversion to a specific format
		if (in_array('date_once', $which)) {
			$ret['date_once'] = "M d Y, H:i";
		}
		
		// Weekday conversion (number to text)
		if (in_array('date_weekly', $which)) {
			$ret['date_weekly'] = Array(1 => 'Monday', 2 => 'Tuesday', 3 => 'Wednesday', 4 => 'Thursday', 5 => 'Friday', 6 => 'Saturday', 7 => 'Sunday');
		}
		
		// Repeat types & descriptions
		if (in_array('repeat', $which)) {
			$ret['repeat'] = Array(
				'once' => 'Once',
				'daily' => 'Daily',
				'weekly' => 'Weekly',
				'monthly' => 'Monthly',
				'x_minute' => 'Every x minute');
		}

		// Jobs
		if (in_array('job', $which)) {
			$ret['job'] = Array(
				'type' 		=> 'select',
				'name'		=> 'job',
				'values'	=> Array(
					// Reboot
					'reboot' => FALSE,
					// Reconnect WAN
					'reconnect_wan' => FALSE,
					// Interface
					'interface' => Array(
						'type' 		=> 'select',
						'name'		=> 'interface',
						'values'	=> Array(
							'enable' => Array(
								'type'		=> 'select',
								'name'		=> 'enable',
								'values'	=> 'enabled_interfaces',
							),
							'disable' => Array(
								'type'		=> 'select',
								'name'		=> 'disable',
								'values'	=> 'enabled_interfaces',
							),
						),
					),
					// Traffic Shaper
					'shaper' => Array(
						'type' 		=> 'select',
						'name'		=> 'shaper',
						'values'	=> Array(
							'enable' => FALSE,
							'disable' => FALSE,
							'enable_rule' => Array(
								'type'		=> 'select',
								'name'		=> 'enable_rule',
								'values'	=> 'shaper_rule',
							),
							'disable_rule' => Array(
								'type'		=> 'select',
								'name'		=> 'disable_rule',
								'values'	=> 'shaper_rule',
							),
							'set_pipe_bandwidth' => Array(
								'type'		=> 'select',
								'name'		=> 'set_pipe_bandwidth',
								'values'	=> 'shaper_pipe',
								'child'		=> Array(
									'type'		=> 'digit',
									'valid'		=> '0,*',
									'name'		=> 'value',
									'descr'		=> 'Kbit/s',
								),
							),
							'set_queue_weight' => Array(
								'type'		=> 'select',
								'name'		=> 'set_queue_weight',
								'values'	=> 'shaper_queue',
								'child'		=> Array(
									'type'		=> 'digit',
									'valid'		=> '1,100',
									'name'		=> 'value',
								),
							),
						),
					),
					'exec_cmd' => Array(
						'type'		=> 'text',
						'valid'		=> FALSE,
						'name'		=> 'exec_cmd',
					),
				),
			);
		}
		
		// Job targets
		if (in_array('target', $which)) {
			$ret['target'] = Array(
				'enabled_interfaces'	=> croen_get_enabled_interfaces(0),
				'shaper_rule'			=> croen_get_shaper('rule'),
				'shaper_pipe'			=> croen_get_shaper('pipe'),
				'shaper_queue'			=> croen_get_shaper('queue'),
			);
		}
		
		// Job descriptions
		if (in_array('descr', $which)) {
			$ret['descr'] = Array(
				// Jobs
				'reboot' 				=> 'Reboot system',
				'reconnect_wan'			=> 'Reconnect WAN',
				'interface'				=> 'Interface',
				'shaper'				=> 'Traffic shaper',
				'exec_cmd'				=> 'Execute command',
				// Instructions
				'enable'				=> 'Enable',
				'disable'				=> 'Disable',
				'enable_rule'			=> 'Enable rule',
				'disable_rule'			=> 'Disable rule',
				'set_pipe_bandwidth'	=> 'Set pipe bandwidth',
				'set_queue_weight'		=> 'Set queue weight',
			);
		}

		return $ret;
	}

	// Parse croen.tmp and return job data
	function croen_parse_tmpfile($file = FALSE) {
		// Default config
		croen_set_default_config();
	
		global $config, $g;
		if (!$file) {
			$file = $g['tmp_path'].'/croen.tmp';
		}
		$arrow = ' -> ';
		
		// Get contents of temp file
		$id = (file_exists($file) ? file_get_contents($file) : FALSE);
		if ($id !== FALSE && ctype_digit(trim($id)) && isset($config['croen']['jobset'][trim($id)]['job']) && is_array($config['croen']['jobset'][trim($id)]['job'])) {
			foreach ($config['croen']['jobset'][trim($id)]['job'] AS $i => $job) {
				// Check for existance
				$data = croen_job_exists($job['name'], Array('req', 'descr'), (isset($job['target']) ? Array('target' => $job['target']) : Array()));
				if ($data && is_array($data)) {
					// Validate job
					if (!(($data['req']['target'] && !isset($job['target'])) || ($data['req']['target'] && !$data['descr']['target']) || ($data['req']['value'] && !isset($job['value'])))) {
						$descr = '';
						foreach ($data['descr']['job'] AS $k => $v) {
							$descr .= $v.$arrow;
						}
						if (isset($job['target'])) {
							$jobs[$i]['target'] = ($job['target'] == $data['descr']['target'] ? Array($job['target']) : Array($job['target'], $data['descr']['target']));
							$descr .= $data['descr']['target'].$arrow;
						}
						if (isset($job['value'])) {
							$jobs[$i]['value'] = $job['value'];
							$descr .= $job['value'].$arrow;
						}
						$jobs[$i]['name'] = Array($job['name'], substr($descr, 0, (strlen($arrow))*-1));
					}
				}
			}
			if (isset($jobs)) {
				return Array(
					'id' 		=> $id,
					'descr'		=> (isset($config['croen']['jobset'][trim($id)]['descr']) && !empty($config['croen']['jobset'][trim($id)]['descr']) ? $config['croen']['jobset'][trim($id)]['descr'] : FALSE),
					'syslog'	=> isset($config['croen']['jobset'][trim($id)]['syslog']),
					'jobs' 		=> $jobs
				);
			}
		}
		
		return FALSE;
	}

	// Check whether a job exists or not
	function croen_job_exists($name, $more = Array(), $var = Array()) {
		// more: req, descr
		// var: target (get description of target)

		// Requirements
		if (in_array('req', $more)) {
			$req = Array('target' => FALSE, 'value' => FALSE);
		}
		// Description
		if (in_array('descr', $more)) {
			$d = croen_vars(Array('descr')); $d = $d['descr'];
			$descr = Array();
		}
		// Job
		$job = croen_vars(Array('job'));
		$job = $job['job']['values'];
		
		$name = explode("-", $name);
		// Check if job is existing by following the array tree
		foreach ($name AS $v) {
			if (isset($job[$v])) {
				// Job description
				if (isset($descr)) {
					$descr['job'][$v] = (isset($d[$v]) ? $d[$v] : $v);
				}
				// Type: Select
				if ($job[$v]['type'] == 'select' && isset($job[$v]['values']) && !is_array($job[$v]['values'])) {
					if ($req) {
						$req['target'] = TRUE;
					}
					// Target description
					if (isset($descr) && isset($var['target'])) {
						$t = croen_vars(Array('target'));
						$descr['target'] = (isset($t['target'][$job[$v]['values']][$var['target']]) ? $t['target'][$job[$v]['values']][$var['target']] : $var['target']);
					}
					// Child type: Text || Digit
					if (isset($job[$v]['child']) && ($job[$v]['child']['type'] == 'digit' || $job[$v]['child']['type'] == 'text')) {
						if ($req) {
							$req['value'] = TRUE;
						}
						// Input description
						if (isset($descr) && isset($job[$v]['child']['descr'])) {
							$descr['input'] = $job[$v]['child']['descr'];
						}
					}
				}
				// Type: Text || Digit
				elseif ($job[$v]['type'] == 'digit' || $job[$v]['type'] == 'text') {
					if ($req) {
						$req['value'] = TRUE;
					}
					// Input description
					if (isset($descr) && isset($job[$v]['child']['descr'])) {
						$descr['input'] = $job[$v]['child']['descr'];
					}
				}
				$job = (isset($job[$v]['values']) && is_array($job[$v]['values']) ? $job[$v]['values'] : FALSE);
			} else {
				return FALSE;
			}
		}

		// Return
		if (isset($req)) {
			$ret['req'] = $req;
		}
		if (isset($descr)) {
			$ret['descr'] = $descr;
		}
		return (isset($ret) ? $ret : TRUE);
	}

	// Get shaper configuration
	function croen_get_shaper($what) {
		global $config;
		$list = Array();
		
		if (isset($config['shaper'][$what]) && is_array($config['shaper'][$what])) {
			foreach ($config['shaper'][$what] AS $v) {
				if (isset($v['descr']) && is_string($v['descr']) && $v['descr'] != '' && !in_array($v['descr'], $list)) {
					$list[] = $v['descr'];
				}
			}
		}
		
		return $list;
	}

	// Get assigned & enabled interfaces
	function croen_get_enabled_interfaces($mode = 0) {
		// Mode 0: Return interface name => descr of interface
		// Mode 1: Return verbose list with wireless parents
		// Mode 2: Return verbose list without wireless parents

		global $config;
		$iflist = Array();
		$ifdeplist = Array();
		
		// Get interfaces which are enabled
		if (is_array($config['interfaces'])) {
			foreach ($config['interfaces'] as $ifname => $ifinfo) {
				if ($ifname == 'lan' || $ifname == 'wan' || isset($ifinfo['enable'])) {
					// Mode 0
					if ($mode == 0) {
						$iflist[$ifname] = ($ifname == 'lan' ? 'LAN' :
										   ($ifname == 'wan' ? 'WAN' :
										   (isset($ifinfo['descr']) ? $ifinfo['descr'] : $ifname)));

					// Mode 1/2
					} else {
						$iflist[$ifname] = Array('if' => $ifinfo['if'], 'descr' => (isset($ifinfo['descr']) ? $ifinfo['descr'] : strtoupper($ifname)));
						// Add parent if wlan
						if ($mode == 1 && preg_match("/^wlan/", $ifinfo['if']) && isset($config['wlans']['wlan'][substr($ifinfo['if'], 4)])) {
							$iflist[$ifname]['parent'] = $config['wlans']['wlan'][substr($ifinfo['if'], 4)]['if'];
							// Add dependency list
							if (!isset($ifdeplist[$config['wlans']['wlan'][substr($ifinfo['if'], 4)]['if']])) {
								$ifdeplist[$config['wlans']['wlan'][substr($ifinfo['if'], 4)]['if']] = croen_get_wireless_interface_dependencies($config['wlans']['wlan'][substr($ifinfo['if'], 4)]['if']);
							}
						}
					}
				}
			}
		}

		return ($mode == 1 ? Array('interfaces' => $iflist, 'wireless_interface_dependencies' => $ifdeplist) : $iflist);
	}

	// Get wireless interface dependencies of a parent interface
	function croen_get_wireless_interface_dependencies($ifname) {
		global $config;
		if (is_array($config['wlans']['wlan'])) {
			foreach ($config['wlans']['wlan'] AS $ifid => $ifinfo) {
				if ($ifinfo['if'] == $ifname) {
					$iflist[] = 'wlan'.$ifid;
				}
			}
		}
		return (isset($iflist) ? $iflist : Array());
	}

	// Update target(s) of job(s)
	function croen_update_job($jobName, $oldTarget = FALSE, $newTarget = FALSE) {
		// Default config
		croen_set_default_config();
		global $config;

		foreach ($config['croen']['jobset'] AS $jobset_id => $jobset) {
			if (isset($jobset['job']) && is_array($jobset['job'])) {
				foreach ($jobset['job'] AS $job_id => $job) {
					if ((!is_array($jobName) && $job['name'] == $jobName) || (is_array($jobName) && in_array($job['name'], $jobName))) {
						if ($oldTarget !== FALSE && $newTarget !== FALSE && isset($job['target']) && $job['target'] == $oldTarget) {
							// Set target to new target
							$config['croen']['jobset'][$jobset_id]['job'][$job_id]['target'] = $newTarget;
						} elseif ($oldTarget === FALSE || ($oldTarget !== FALSE && isset($job['target']) && $job['target'] == $oldTarget)) {
							// Unset job
							unset($config['croen']['jobset'][$jobset_id]['job'][$job_id]);
							if (empty($config['croen']['jobset'][$jobset_id]['job'])) {
								// Unset jobset if no jobs exist
								unset($config['croen']['jobset'][$jobset_id]);
							}
						}
					}
				}
			}
		}
	}
	
	// Update assigned interfaces
	/*function croen_update_interfaces($newPorts) {
		$jobName = Array('interface-enable', 'interface-disable');
		// Unset non-interface vars
		foreach ($newPorts AS $ifname => $ignore) {
			if ($ifname != 'lan' && $ifname != 'wan' && substr($ifname, 0, 3) != 'opt') {
				unset($newPorts[$ifname]);
			}
		}

		echo '<pre>'.print_r(croen_get_enabled_interfaces(2))."\n".print_r($newPorts).'</pre>'; // debug
		
		foreach (croen_get_enabled_interfaces(2) AS $ifname => $ifinfo) {
			if (isset($newPorts[$ifname]) && $newPorts[$ifname] != $ifinfo['if']) {
				// Update job where ifname = target
				croen_update_job($jobName, $ifname, $newPorts[$ifname]);
			} else {
				// Unset job where ifname = target
				croen_update_job($jobName, $ifname);
			}
		}
	}*/
	
	// Write syslog entry
	function croen_syslog($m) {
		define_syslog_variables();
		openlog("croen", FALSE, LOG_NOTICE);
		syslog(LOG_NOTICE, trim($m));
		closelog();
	}

?>
