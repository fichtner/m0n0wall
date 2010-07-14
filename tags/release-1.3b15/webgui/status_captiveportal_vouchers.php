#!/usr/local/bin/php
<?php 
/*
    $Id$
    
    Copyright (C) 2007 Marcel Wiget <mwiget@mac.com>.
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

$pgtitle = array("Status", "Captive portal");
require("guiconfig.inc");
?>
<?php include("fbegin.inc"); ?>
<?php

function clientcmp($a, $b) {
    global $order;
    return strcmp($a[$order], $b[$order]);
}

voucher_lock();

if (!is_array($config['voucher']['roll'])) {
    $config['voucher']['roll'] = array();
}
$a_roll = &$config['voucher']['roll'];

$db = array();

foreach($a_roll as $rollent) {
    $roll = $rollent['number'];
    $minutes = $rollent['minutes'];
    $active_vouchers = voucher_read_active_db($roll);
    foreach($active_vouchers as $voucher => $line) {
        list($timestamp, $minutes) = explode(",", $line);
        $remaining = (($timestamp + 60*$minutes) - time());
        if ($remaining > 0) {
            $dbent[0] = $voucher;
            $dbent[1] = $roll;  
            $dbent[2] = $timestamp;
            $dbent[3] = intval($remaining/60);
            $dbent[4] = $timestamp + 60*$minutes; // expires at 
            $db[] = $dbent;
        }
    }
}

if ($_GET['order']) { 
    $order = $_GET['order'];
    usort($db, "clientcmp");
}
voucher_unlock();
?>

<form action="status_captiveportal_vouchers.php" method="post" enctype="multipart/form-data" name="iform" id="iform">
<table width="100%" border="0" cellpadding="0" cellspacing="0" summary="tab pane">
<tr><td class="tabnavtbl">
<ul id="tabnav">
<?php 
$tabs = array('Users' => 'status_captiveportal.php',
        'Active Vouchers' => 'status_captiveportal_vouchers.php',
        'Voucher Rolls' => 'status_captiveportal_voucher_rolls.php',
        'Test Vouchers' => 'status_captiveportal_test.php');
    dynamic_tab_menu($tabs);
?> 
</ul>
</td></tr>
<tr>
<td class="tabcont">

<table width="100%" border="0" cellpadding="0" cellspacing="0" summary="content pane">
  <tr>
    <td class="listhdrr"><a href="?order=0&showact=<?=$_GET['showact'];?>">Voucher</a></td>
    <td class="listhdrr"><a href="?order=1&showact=<?=$_GET['showact'];?>">Roll</a></td>
    <td class="listhdrr"><a href="?order=2&showact=<?=$_GET['showact'];?>">Activated at</a></td>
    <td class="listhdrr"><a href="?order=3&showact=<?=$_GET['showact'];?>">Expires in</a></td>
    <td class="listhdr"><a href="?order=4&showact=<?=$_GET['showact'];?>">Expires at</a></td>
    <td class="list"></td>
  </tr>
<?php foreach ($db as $dbent): ?>
  <tr>
    <td class="listlr"><?=$dbent[0];?></td>
    <td class="listr"><?=$dbent[1];?></td>
    <td class="listr"><?=htmlspecialchars(date("m/d/Y H:i:s", $dbent[2]));?></td>
    <td class="listr"><?=$dbent[3];?> min</td>
    <td class="listr"><?=htmlspecialchars(date("m/d/Y H:i:s", $dbent[4]));?></td>
    <td class="list"></td>
  </tr>
<?php endforeach; ?>
</table>
</td>
</tr>
</table>
</form>
<p>
<?php include("fend.inc"); ?>
