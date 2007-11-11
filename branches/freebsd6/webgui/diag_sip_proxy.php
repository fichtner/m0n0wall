#!/usr/local/bin/php
<?php 
/*
	$Id$
	part of m0n0wall (http://m0n0.ch/wall)
	
	Copyright (C) 2007 Marcel Wiget <mwiget@mac.com>
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

$pgtitle = array("Diagnostics", "SIP Proxy");

require("guiconfig.inc");
?>
<?php include("fbegin.inc"); ?>
<?php

flush();

function adjust_gmt($dt) {
	$ts = strtotime($dt . " GMT");
	return strftime("%Y/%m/%d %H:%M:%S", $ts);
}

$fp = @fopen("{$g['vardb_path']}/siproxd_registrations","r");

if ($fp && file_exists("{$g['varrun_path']}/siproxd.pid")):

$return = array();

$i = 0;
$reg = array();

while ($line = fgets($fp)) {
	$matches = "";

	if (preg_match("/(.*):(.*):(.*)/", $line, $matches)) {
        if ("***" == $matches[1] && "1" == $matches[2]) {
            // we have an active registration
            $reg[$i]['expires'] = $matches[3];
            // get details about this registration
            $reg[$i]['true_url_scheme'] = fgets($fp);
            $reg[$i]['true_url_username'] = fgets($fp);
            $reg[$i]['true_url_host'] = fgets($fp);
            $reg[$i]['true_url_port'] = fgets($fp);
            if ("\n" == $reg[$i]['true_url_port']) $reg[$i]['true_url_port'] = "5060";
            $reg[$i]['masq_url_scheme'] = fgets($fp);
            $reg[$i]['masq_url_username'] = fgets($fp);
            $reg[$i]['masq_url_host'] = fgets($fp);
            $reg[$i]['masq_url_port'] = fgets($fp);
            if ("\n" == $reg[$i]['masq_url_port']) $reg[$i]['masq_url_port'] = "5060";
            $reg[$i]['reg_url_scheme'] = fgets($fp);
            $reg[$i]['reg_url_username'] = fgets($fp);
            $reg[$i]['reg_url_host'] = fgets($fp);
            $reg[$i]['reg_url_port'] = fgets($fp);
            if ("\n" == $reg[$i]['reg_url_port']) $reg[$i]['reg_url_port'] = "5060";
        }
    }
    $i++;
}

fclose($fp);

?>
<table width="100%" border="0" cellpadding="0" cellspacing="0">
  <tr>
    <td class="listhdrr" colspan=3>True URL</td>
    <td class="listhdrr" colspan=3>Masqueraded URL</td>
    <td class="listhdrr" colspan=3>Registered URL</td>
    <td class="listhdrr"></td>
  </tr>
  <tr>
    <td class="listhdrr">Username</td>
    <td class="listhdrr">Host</td>
    <td class="listhdrr">Port</td>
    <td class="listhdrr">Username</td>
    <td class="listhdrr">Host</td>
    <td class="listhdrr">Port</td>
    <td class="listhdrr">Username</td>
    <td class="listhdrr">Host</td>
    <td class="listhdrr">Port</td>
    <td class="listhdr">Expires</td>
    <td class="list"></td>
	</tr>
<?php
foreach ($reg as $entry) {
		echo "<tr>\n";
		echo "<td class=\"listlr\">{$entry['true_url_username']}</td>\n";
		echo "<td class=\"listr\">{$entry['true_url_host']}</td>\n";
		echo "<td class=\"listr\">{$entry['true_url_port']}</td>\n";
		echo "<td class=\"listr\">{$entry['masq_url_username']}</td>\n";
		echo "<td class=\"listr\">{$entry['masq_url_host']}</td>\n";
		echo "<td class=\"listr\">{$entry['masq_url_port']}</td>\n";
		echo "<td class=\"listr\">{$entry['reg_url_username']}</td>\n";
		echo "<td class=\"listr\">{$entry['reg_url_host']}</td>\n";
		echo "<td class=\"listr\">{$entry['reg_url_port']}</td>\n";
		echo "<td class=\"listr\">" . strftime("%Y/%m/%d %H:%M:%S", $entry['expires']) . "</td>\n";
		echo "</tr>\n";
}
?>
</table>
<br>
<?php else: ?>
<strong>No SIP registration file found. Is the SIP service active?</strong>
<?php endif; ?>
<?php include("fend.inc"); ?>
