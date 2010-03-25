#!/usr/local/bin/php
<?php 
/*
	$Id: diag_logs.php 288 2008-07-26 20:10:05Z mwiget $
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
exec("/usr/bin/ntpq -c 'lpeers'",$peerlist);
foreach($peerlist as $peerline){
	check $peerline for GPS
}
$pgtitle = array("Diagnostics", "NTP");
require("guiconfig.inc");
include("fbegin.inc"); ?>
<style media="screen" type="text/css">
<!--
pre {
   margin: 0px;
   font-family: courier new, courier;
   font-weight: normal;
   font-size: 9pt;
}
-->

</style>
<table width="100%" border="0" cellpadding="0" cellspacing="0" summary="tab pane">
  <tr><td class="tabnavtbl">
  <ul id="tabnav">
<?php 
   	$tabs = array('NTP Server' => 'ntp_status.php',
           		  'GPS (x) Information' => 'gps_status.php?gpsid=');
	dynamic_tab_menu($tabs);
?> 
  </ul>
  </td></tr>

   <tr> 
    <td class="tabcont">
	<table id="gpsdata" width="100%" border="0" cellspacing="0" cellpadding="0" summary="content pane"> 
	<tr><td colspan="2" class="listtopic">Quick Peer List</td></tr>
	<tr> 
		<td width="22%" class="vncellt">Data</td> 
		<td width="78%"  class="listr"><pre><?php
	
	exec("/usr/bin/ntpq -c 'lpeers'",$peerlist);
	foreach($peerlist as $peerline){
		echo htmlspecialchars($peerline,ENT_NOQUOTES) . '<br>';
	}
	?></pre>
	</td> 
	</tr>
	<tr><td colspan="8" class="list" height="12"></td></tr>
	<tr><td colspan="2" class="listtopic">Quick Association List</td></tr>
	<tr> 
		<td width="22%" class="vncellt">Data</td> 
		<td width="78%"  class="listr"><pre><?php
	
	exec("/usr/bin/ntpq -c association",$assocl);
	foreach($assocl as $assoc){
		echo htmlspecialchars($assoc,ENT_NOQUOTES) . '<br>';
	}
	?></pre>
	</td> 
	</tr>
<?php
		$assocl[0]='00000000000000';
		foreach($assocl as $assoc){
			$associd=substr($assoc,4,5);
			if (is_numeric($associd)){ 
				?>
				<tr><td colspan="8" class="list" height="12"></td></tr>
				<tr><td colspan="2" class="listtopic">Association <?=$associd;?></td></tr>
				<?php
				unset($assocds);
				exec("/usr/bin/ntpq -c 'rv {$associd}'",$assocds);
				foreach($assocds as $assocd){
					?>
					<tr> 
					<td width="22%" class="vncellt"></td> 
					<td width="78%" class="listr"><?=$assocd;?></td> 
					</tr>
				<?php
				}
			}
		}
	?>
	   </table>          
</table>
<?php include("fend.inc"); ?>
