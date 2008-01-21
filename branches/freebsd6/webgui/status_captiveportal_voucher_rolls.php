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

if (!is_array($config['voucher']['roll'])) {
    $config['voucher']['roll'] = array();
}
$a_roll = &$config['voucher']['roll'];

?>

<form action="status_captiveportal_voucher_rolls.php" method="post" enctype="multipart/form-data" name="iform" id="iform">
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
     <td class="listhdrr">Roll#</td>
     <td class="listhdrr">Minutes/Ticket</td>
     <td class="listhdrr"># of Tickets</td>
     <td class="listhdrr">Comment</td>
     <td class="listhdrr">used</td>
     <td class="listhdrr">active</td>
     <td class="listhdr">ready</td>
  </tr>
<?php 
    voucher_lock();
    $i = 0; foreach($a_roll as $rollent): 
    $used = voucher_used_count($rollent['number']);
    $active = count(voucher_read_active_db($rollent['number']),$rollent['minutes']);
    $ready = $rollent['count'] - $used;
?>
  <tr>
    <td class="listlr">
    <?=htmlspecialchars($rollent['number']); ?>&nbsp;
    </td>
    <td class="listr">
    <?=htmlspecialchars($rollent['minutes']);?>&nbsp;
    </td>
    <td class="listr">
    <?=htmlspecialchars($rollent['count']);?>&nbsp;
    </td>
    <td class="listr">
    <?=htmlspecialchars($rollent['comment']); ?>&nbsp;
    </td>
    <td class="listr">
    <?=htmlspecialchars($used); ?>&nbsp;
    </td>
    <td class="listr">
    <?=htmlspecialchars($active); ?>&nbsp;
    </td>
    <td class="listr">
    <?=htmlspecialchars($ready); ?>&nbsp;
    </td>
  </tr>
	<?php $i++; endforeach; voucher_unlock(); ?>
</table>     
</td>
</tr>
</table>     
</form>
</p>
<?php include("fend.inc"); ?>
