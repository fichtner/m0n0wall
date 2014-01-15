#!/usr/local/bin/php
<?php 
/*
	$Id$
	part of m0n0wall (http://m0n0.ch/wall)
	
	Copyright (C) 2005-2006 Paul Taylor (paultaylor@winndixie.com) and Manuel Kasper <mk@neon1.net>.
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

$pgtitle = array("Diagnostics", "Firewall states");
require("guiconfig.inc");
?>
<?php

flush();

function natsort2d( &$arrIn, $index = null )
{
   
   $arrTemp = array();
   $arrOut = array();
   
   if (is_array($arrIn)) {
	   foreach ( $arrIn as $key=>$value ) {
		   
		   reset($value);
		   $arrTemp[$key] = is_null($index)
							   ? current($value)
							   : $value[$index];
	   }
   }
   
   natsort($arrTemp);
   
   foreach ( $arrTemp as $key=>$value ) {
       $arrOut[$key] = $arrIn[$key];
   }
   
   $arrIn = $arrOut;
   
}

// sfilter and dfilter allow setting of source and dest IP filters
// on the output.  $filterPassThru allows these source and dest 
// filters to be passed on in the column sorting links.
if (($_GET['sfilter']) or ($_GET['dfilter'])) {
	
	$filter = '';
	if ($_GET['sfilter']) {
		if (is_ipaddr4or6($_GET['sfilter'])) {
			$filter = ' -S ' . $_GET['sfilter'];
			$filterPassThru = '&sfilter=' . urlencode($_GET['sfilter']);
		}
		else 
			unset ($_GET['sfilter']);
	}
	if ($_GET['dfilter']) {
		if (is_ipaddr4or6($_GET['dfilter']))
		{
			$filter = ' -D ' . $_GET['dfilter'];
			$filterPassThru = '&dfilter=' . urlencode($_GET['dfilter']);
		}
		else 
			unset ($_GET['dfilter']);
	}
	
}

if ($_GET['ip']=='6') {
	$fd = popen("/sbin/ipfstat -t -6" . $filter, "r");
	$filterPassThru .= "&ip=6";
} else {
	$fd = popen("/sbin/ipfstat -t " . $filter, "r");
}

// See if the user has set a limit to the number of entries...  
if (isset($config['diag']['ipfstatentries']))
	$linelimit = $config['diag']['ipfstatentries']; 
else
	$linelimit = 300;

$count = 0;
while (!feof($fd)) {
	$line = trim(fgets($fd));
	if (!$line)
		continue;
	
//Source IP             Destination IP         ST   PR   #pkts    #bytes       ttl
//68.16.26.144,1633     167.219.90.224,443    4/4  tcp  366724 370351656   2:30:00
//      0                        1              2   3      4       5          6
	$split = explode("\t", trim($line));				
	$srcTmp = $split[0];
	$data[$count]['srcip'] = stripPort($srcTmp);
	$data[$count]['srcport'] = stripPort($srcTmp,true);
	$dstTmp = $split[1];
	$data[$count]['dstip'] = stripPort($dstTmp);
	$data[$count]['dstport'] = stripPort($dstTmp,true);
	$data[$count]['protocol'] = $split[3];
	$data[$count]['packets'] = $split[4];
	$data[$count]['bytes'] = $split[5];
	$timeTmp = trim($split[6]);
	$timeLen = strlen($timeTmp);
	switch ($timeLen) {
	case 4: 
		$data[$count]['ttl'] = strtotime("0:0".$timeTmp);
		break;
	case 5: 
		$data[$count]['ttl'] = strtotime("0:".$timeTmp);
		break;
	case 7: 
		$data[$count]['ttl'] = strtotime($timeTmp);
		break;
	default :
	// Debug logic, in case there is an unforseen issue
		/*echo $line . "<br>";
		echo $linelimit . "<br>";
		echo $timeTmp . "<br>";*/
		break;
	}
	$count++;
	if ($linelimit == $count) {
		// We've got all the data the user wanted to see - drop out of the foreach.
		break;
	}
}

pclose($fd);

// Clear the statistics snapshot files, which track the packets and bytes of connections

if (isset($_GET['clear']))
{
	if ($_GET['ip']=='6') {
		if (file_exists('/tmp/packets6'))
			unlink('/tmp/packets6');
		if (file_exists('/tmp/bytes6'))
			unlink('/tmp/bytes6');
	} else {
		if (file_exists('/tmp/packets'))
			unlink('/tmp/packets');
		if (file_exists('/tmp/bytes'))
			unlink('/tmp/bytes');
	}
		
	// Redirect so we don't hit "clear" every time we refresh the screen.
	header("Location: diag_ipfstat.php?".$filterPassThru);
	exit;
}

// Create a new set of stats snapshot files
if (isset($_GET['new']))
{
	$packets = array();
	$bytes = array();
	
	// Create variables to let us later quickly access this data
	if (is_array($data)) {
		foreach ($data as $row) {
			$packets[$row['srcip']][$row['srcport']][$row['dstip']][$row['dstport']][$row['protocol']] = $row['packets'];
			$bytes[$row['srcip']][$row['srcport']][$row['dstip']][$row['dstport']][$row['protocol']] = $row['bytes'];
		}
	}

	// Write the files out
	writeStats("packets",$packets);
	writeStats("bytes",$bytes);
	
	// If we're in view mode, pass that on.
	if (isset($_GET['view']))
		$filterPassThru .= "&view=1";
	
	// Redirect so we don't hit "new" every time we refresh the screen.
	header("Location: diag_ipfstat.php?order=bytes&sort=des".$filterPassThru);
	exit;
}
	
// View the delta from the last snapshot against the current data.
if (isset($_GET['view']))
{

	// Read the stats data files
	readStats("packets",$packets);
	readStats("bytes",$bytes);

	if (is_array($data)) {
		foreach ($data as $key => $row) {
			if (isset($packets[$row['srcip']][$row['srcport']][$row['dstip']][$row['dstport']][$row['protocol']]))
			{
				if (isset($bytes[$row['srcip']][$row['srcport']][$row['dstip']][$row['dstport']][$row['protocol']]))
				{
					$tempPackets = $data[$key]['packets'] - $packets[$row['srcip']][$row['srcport']][$row['dstip']][$row['dstport']][$row['protocol']];
					$tempBytes = $data[$key]['bytes'] - $bytes[$row['srcip']][$row['srcport']][$row['dstip']][$row['dstport']][$row['protocol']];
					if (($tempPackets > -1) && ($tempBytes > -1))
					{
						$data[$key]['packets'] = $tempPackets;
						$data[$key]['bytes'] = $tempBytes;
					}
				}
			}
			
		}
	}
	
	$filterPassThru .= "&view=1";
	$viewPassThru = "&view=1";
}

// Sort it by the selected order
if ($_GET['order']) {
	natsort2d($data,$_GET['order']);
	if ($_GET['sort'])
	{
		if ($_GET['sort'] == "des")
		$data = array_reverse($data);
	}
}



function writeStats($fname, &$data) {
	if ($_GET['ip']=='6') {
		$fname = "/tmp/" . $fname . "6";
	} else {
		$fname = "/tmp/" . $fname;
	}
	if (file_exists($fname))
		unlink($fname);
	$file = fopen($fname, 'a');
	fwrite($file, serialize($data)); 
	fclose($file);
}

function readStats($fname, &$data) {
	if ($_GET['ip']=='6') {
		$fname = "/tmp/" . $fname . "6";
	} else {
		$fname = "/tmp/" . $fname;
	}
	if (file_exists($fname))
	{
		$file = fopen($fname,'r');
		$data = unserialize(fread($file, filesize($fname)));
		fclose($file);			
	}
}

function sortOrder($column) {
	
	if ($_GET['order'] == $column) {
		if ($_GET['sort'] == 'des')
			return "&amp;sort=asc";	
		return "&amp;sort=des";
	}
	else 
		return "&amp;sort=asc";	
}

function stripPort($ip, $showPort = false) {
	if (!$showPort) {
		if (strpos($ip,',') > 0)
			return substr($ip,0,strpos($ip,","));
		else 
			return ($ip);
	}
	else {
		if (strpos($ip,',') > 0) {
			return substr($ip,(strpos($ip,",")+1));
		}
		else 
			return "&nbsp;";
	}
}

function displayIP($ip, $col) {
	
	global $viewPassThru;
	
	switch ($col) {
		case 'srcip':
			if ($_GET['sfilter']) {
				if ($_GET['sfilter'] == $ip)
					return $ip;
			}
			else {
				if ($_GET['ip']=='6') {
					return '<a href="?ip=6&sfilter='.$ip.$viewPassThru.'">'. $ip .'</a>';
				} else {
					return '<a href="?sfilter='.$ip.$viewPassThru.'">'. $ip .'</a>';
				}
			}
			break;
	
		case 'dstip':
			if ($_GET['dfilter']) {
				if ($_GET['dfilter'] == $ip)
					return $ip;				
			}
			else {
				if ($_GET['ip']=='6') {
					return '<a href="?ip=6&dfilter='.$ip.$viewPassThru.'">'. $ip .'</a>';
				} else {
					return '<a href="?dfilter='.$ip.$viewPassThru.'">'. $ip .'</a>';
				}
			}			
			break;
	}
	
}

// Get timestamp of snapshot file, if it exists, for display later.
if ($_GET['ip']=='6') {
	$fname = "/tmp/packets6";
} else {
	$fname = "/tmp/packets";
}
if (!(file_exists($fname))) {
	$lastSnapshot = "Never"; 
}
else { 
	$lastSnapshot = strftime("%m/%d/%y %H:%M:%S",filectime($fname));
}

// Moved this down here due to the potential for redirects, up above.
include("fbegin.inc");

// Produce proper HTML
$filterPassThru = str_replace("&", "&amp;", $filterPassThru);
if (ipv6enabled()) {

?>
	<table width="100%" border="0" cellpadding="0" cellspacing="0" summary="tab pane">
	  <tr><td class="tabnavtbl">
	  <ul id="tabnav">
<?php
if ($_GET['ip'] == '6') { 
?>
	    <li class="tabinact1"><a href="diag_ipfstat.php">IPv4</a></li>
		<li class="tabact">IPv6</li>
<?php 
} else {
?>
	    <li class="tabact">IPv4</li>
		<li class="tabinact1"><a href="diag_ipfstat.php?ip=6">IPv6</a></li>	
	<?php	
}
?>
	  </ul>
	  </td></tr>
	  <tr> 
	    <td class="tabcont">
<?php
}
?>

<table width="100%" border="0" cellpadding="0" cellspacing="0">
  <tr>
  	<td class="listhdrr" colspan="8">Statistics snapshot control</td>
  </tr>
  <tr>
    <?php if (($lastSnapshot!='Never') && (!isset($_GET['view']))) :?>  
    <td class="listlr"><a href="?view=1&amp;order=bytes&amp;sort=des<?=$filterPassThru;?>">View delta</a></td>
    <td class="listr"><a href="?new=1<?=$filterPassThru;?>">Start new</a></td>
    <td class="listr"><a href="?clear=1<?=$filterPassThru;?>">Clear snapshot</a></td>
	<td class="listr" colspan="5" align="right">Last statistics snapshot: <?=$lastSnapshot;?></td>    
    <?php endif ?>
    <?php if (($lastSnapshot!='Never') && (isset($_GET['view']))) :?>  
    <td class="listlr"><a href="?new=1<?=$filterPassThru;?>">Start new</a></td>
    <td class="listr"><a href="?clear=1<?=$filterPassThru;?>">Clear</a></td>
	<td class="listr" colspan="6" align="right"><span class="red">Viewing delta of statistics snapshot: <?=$lastSnapshot;?></span></td>    
    <?php endif ?>    
    <?php if ($lastSnapshot=='Never') :?>
    <td class="listlr"><a href="?new=1<?=$filterPassThru;?>">Start new</a></td>
    <td class="listr" colspan="7" align="right">Last statistics snapshot: <?=$lastSnapshot;?></td>    
    <?php endif ?>
  </tr>
  <tr>
  	<td colspan="8">&nbsp;</td>
  </tr>
  <tr>
    <td class="listhdrr"><a href="?order=srcip<?=sortOrder('srcip');echo $filterPassThru;?>">Source</a></td>
    <td class="listhdrr" align="right"><a href="?order=srcport<?=sortOrder('srcport');echo $filterPassThru;?>">Port</a></td>
    <td class="listhdrr"><a href="?order=dstip<?=sortOrder('dstip');echo $filterPassThru;?>">Destination</a></td>
    <td class="listhdrr" align="right"><a href="?order=dstport<?=sortOrder('dstport');echo $filterPassThru;?>">Port</a></td>
    <td class="listhdrr"><a href="?order=protocol<?=sortOrder('protocol');echo $filterPassThru;?>">Protocol</a></td>
    <td class="listhdrr" align="right"><a href="?order=packets<?=sortOrder('packets');echo $filterPassThru;?>">Packets</a></td>
    <td class="listhdrr" align="right"><a href="?order=bytes<?=sortOrder('bytes');echo $filterPassThru;?>">Bytes</a></td>
    <td class="listhdr" align="right"><a href="?order=ttl<?=sortOrder('ttl');echo $filterPassThru;?>">TTL</a></td>
    <td class="list"></td>
  </tr>
<?php if (is_array($data)): foreach ($data as $entry): ?>
  <tr>
    <td class="listlr"><?=displayIP($entry['srcip'],'srcip');?></td>
    <td class="listr"><?=$entry['srcport'];?></td>
    <td class="listr"><?=displayIP($entry['dstip'],'dstip');?></td>
    <td class="listr"><?=$entry['dstport'];?></td>
    <td class="listr"><?=$entry['protocol'];?></td>
    <td class="listr" align="right"><?=$entry['packets'];?></td>
    <td class="listr" align="right"><?=$entry['bytes'];?></td>
    <td class="listr" align="right"><?=preg_replace("/^((00:0)|(00:)|(0))/","",strftime('%H:%M:%S',$entry['ttl']));?></td>    
  </tr>
<?php endforeach; endif; ?>
</table>
<br><strong>Firewall connection states displayed: <?=count($data);?></strong>
<?php if ($filterPassThru): ?>
<p>
<form action="diag_ipfstat.php" method="GET">
<input type="hidden" name="order" value="bytes">
<input type="hidden" name="sort" value="des">
<input type="submit" class="formbtn" value="Unfilter View">
</form>
</p>

<?php endif;
if (ipv6enabled()) {
?>
	</td>
	</tr>
	</table>
<?php
}?>
<?php include("fend.inc"); ?>
