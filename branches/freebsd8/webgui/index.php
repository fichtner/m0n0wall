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

$pgtitle = array("m0n0wall webGUI");
$pgtitle_omit = true;
require("guiconfig.inc");

/* find out whether there's hardware encryption (hifn) */
unset($hwcrypto);
$dmesg = system_get_dmesg_boot();
if (preg_match("/^hifn.: (.*?),/m", $dmesg, $matches))
	$hwcrypto = $matches[1];
if (preg_match("/^padlock.: (.*?)/m", $dmesg, $matches))
	$hwcrypto = "VIA Padlock";
if (preg_match("/^glxsb.: (.*?)/m", $dmesg, $matches))
	$hwcrypto = "AMD Geode LX Security Block";
$specplatform = system_identify_specific_platform();
if (preg_match("/^CPU.*/m", $dmesg, $matches) )
	$cpudetail = " - " . $matches[0];

if ($_POST) {
	$config['system']['notes'] = base64_encode($_POST['notes']);
	write_config();
	header("Location: index.php");
	exit;
}
	exec("export QUERY_STRING=cpu;export REQUEST_METHOD=GET;/usr/local/www/stats.cgi" ,$cpuutil);
    $cpuu = $cpuutil[2];

	exec("/sbin/sysctl -n vm.stats.vm.v_active_count vm.stats.vm.v_inactive_count " .
    	"vm.stats.vm.v_wire_count vm.stats.vm.v_cache_count vm.stats.vm.v_free_count", $memory);
    $totalMem = $memory[0] + $memory[1] + $memory[2] + $memory[3] + $memory[4];
    $freeMem = $memory[4];
    $usedMem = $totalMem - $freeMem;
    $memUsage = round(($usedMem * 100) / $totalMem, 0);

?>
<?php include("fbegin.inc"); ?>
<form action="" method="POST">
            <table width="100%" border="0" cellspacing="0" cellpadding="0" summary="content pane">
              <tr align="center" valign="top"> 
                <td height="10" colspan="2">&nbsp;</td>
              </tr>
              <tr align="center" valign="top"> 
                <td height="170" colspan="2"><img src="logobig.gif" width="520" height="149" alt=""></td>
              </tr>
              <tr> 
                <td colspan="2" class="listtopic">System information</td>
              </tr>
              <tr> 
                <td width="25%" class="vncellt">Name</td>
                <td width="75%" class="listr">
                  <?php echo $config['system']['hostname'] . "." . $config['system']['domain']; ?>
                </td>
              </tr>
              <tr> 
                <td width="25%" valign="top" class="vncellt">Version</td>
                <td width="75%" class="listr"> <strong> 
                  <?php readfile("/etc/version"); ?>
                  </strong>
                  built on 
                  <?php readfile("/etc/version.buildtime"); ?>
                </td>
              </tr>
              <tr> 
                <td width="25%" class="vncellt">Platform</td>
                <td width="75%" class="listr"> 
                  <?=htmlspecialchars($specplatform['descr']); ?> <?=htmlspecialchars($cpudetail); ?> 
                </td>
              </tr><?php if ($hwcrypto): ?>
              <tr> 
                <td width="25%" class="vncellt">Hardware crypto</td>
                <td width="75%" class="listr"> 
                  <?=htmlspecialchars($hwcrypto);?>
                </td>
              </tr><?php endif; ?>
              <tr> 
                <td width="25%" class="vncellt">System Date</td>
                <td width="75%" class="listr"> 
                   <?=exec("/bin/date");?>
                </td>
              </tr>
              <tr> 
                <td width="25%" class="vncellt">Uptime</td>
                <td width="75%" class="listr"> 
                  <?php
				  	exec("/sbin/sysctl -n kern.boottime", $boottime);
					preg_match("/sec = (\d+)/", $boottime[0], $matches);
					$boottime = $matches[1];
					$uptime = time() - $boottime;
					
					if ($uptime > 60)
						$uptime += 30;
					$updays = (int)($uptime / 86400);
					$uptime %= 86400;
					$uphours = (int)($uptime / 3600);
					$uptime %= 3600;
					$upmins = (int)($uptime / 60);
					
					$uptimestr = "";
					if ($updays > 1)
						$uptimestr .= "$updays days, ";
					else if ($updays > 0)
						$uptimestr .= "1 day, ";
					$uptimestr .= sprintf("%02d:%02d", $uphours, $upmins);
					echo htmlspecialchars($uptimestr);
				  ?>
                </td>
              </tr>
              <?php if ($config['lastchange']): ?>
              <tr> 
                <td width="25%" class="vncellt">Last config change</td>
                <td width="75%" class="listr"> 
                  <?=htmlspecialchars(date("D M j G:i:s T Y", $config['lastchange']));?>
                </td>
              </tr>
              <?php endif; ?>
   			  <tr> 
                <td width="25%" class="vncellt">CPU usage</td>
                <td width="75%" class="listr">
                <?php pc_gauge($cpuu,50) ?>
                </td>		
              </tr>
   			  <tr> 
                <td width="25%" class="vncellt">Memory usage</td>
                <td width="75%" class="listr">
			  	<?php pc_gauge($memUsage,50) ?>
			  	</td>
              </tr>
			  <?php
					if ( isset($config['system']['webgui']['mbmon']['enable']) ) {
								if ($config['system']['webgui']['mbmon']['type'] == 'F') {
                                exec("/usr/local/bin/mbmon -f -c1 -r 2>/dev/null ", $mbmonop, $error);
								} else {
								exec("/usr/local/bin/mbmon -c1 -r 2>/dev/null ", $mbmonop, $error);
								}
                                if (!$error){
                        ?>
				<tr>
				<td width="25%" class="vncellt">System Temperatures</td>
				<td width="75%" class="listr">
				<table width="100%" >
                                           <?php
                                                foreach ($mbmonop as $mbival) {
                                                        if (preg_match('/^TEMP/',$mbival)) {
																$mbmonp = explode(':',$mbival);
																?>
				<tr>
				<td align="right" width="15%" class="listsensor">
																<?php
                                                                echo htmlspecialchars(str_replace('TEMP', 'Sensor ',$mbmonp[0]));
																?>
				</td>
				<td align="left" width="85%" class="listsensorval">
																<?php
																echo htmlspecialchars($mbmonp[1]);
																if ($config['system']['webgui']['mbmon']['type'] == 'F') {
                                                                echo '&deg;F</td></tr>';
																} else {
																echo '&deg;C</td></tr>';
																}
																
                                                        }
                                                }
                                          ?>
				</table></td>
				</tr>
				<tr>
				<td width="25%" class="vncellt">System Fans</td>
				<td width="75%" class="listr">
				<table width="100%" >
										<?php
                                                foreach ($mbmonop as $mbival) {
                                                        if (preg_match('/^FAN/',$mbival)) {
																$mbmonp = explode(':',$mbival);
																?>
				<tr>
				<td align="right" width="15%" class="listsensor">
																<?php
                                                                echo htmlspecialchars(str_replace('FAN', 'Fan ',$mbmonp[0]));
																?>
				</td>
				<td align="left" width="85%" class="listsensorval">
																<?php
																echo htmlspecialchars($mbmonp[1]);
                                                                echo ' rpm</td></tr>';
                                                        }
                                                }
                                        ?>
				</table></td>
				</tr>
				<tr>
				<td width="25%" class="vncellt">System Voltages</td>
				<td width="75%" class="listr">
				<table width="100%" >
										<?php
                                                foreach ($mbmonop as $mbival) {
                                                        if (preg_match('/^V\d+/',$mbival)) {
																$oldvolts = array('V33', 'V50P','V12P', 'V12N', 'V50N');
                                                                $nicevolts = array('+3.3v','+5v','+12v','-12v','-5v');
																$mbmonp = explode(':',$mbival);
																?>
				<tr>
				<td align="right" width="15%" class="listsensor">
																<?php
                                                                echo str_replace($oldvolts,$nicevolts,$mbmonp[0]);
																?>
				</td>
				<td align="left" width="85%" class="listsensorval">
																<?php
																echo htmlspecialchars($mbmonp[1]);
                                                                echo ' V</td></tr>';
																
                                                        }
                                                }
                                        ?>
				</table></td>
				</tr>
				<tr>
				<td width="25%" class="vncellt">Core Voltages</td>
				<td width="75%" class="listr">
				<table width="100%" >
										<?php
                                                foreach ($mbmonop as $mbival) {
                                                        if (preg_match('/^VC/',$mbival)) {
																$mbmonp = explode(':',$mbival);
																?>
				<tr>
				<td align="right" width="15%" class="listsensor">
																<?php
                                                                echo htmlspecialchars(str_replace('VC', 'Core ',$mbmonp[0]));
																?></td>
				<td align="left" width="85%" class="listsensorval">
																<?php
																echo htmlspecialchars($mbmonp[1]);
                                                                echo ' V</td></tr>';
																
                                                        }
                                                }
										echo '</table></td></tr>';
                                }
                        }
						?>
              <tr> 
                <td width="25%" class="vncellt" valign="top">Notes</td>
                <td width="75%" class="listr">
                  <textarea name="notes" cols="75" rows="7" id="notes" class="notes"><?=htmlspecialchars(base64_decode($config['system']['notes']));?></textarea><br>
                  <input name="Submit" type="submit" class="formbtns" value="Save">
                </td>
              </tr>
            </table>
</form>
            <?php include("fend.inc"); ?>
