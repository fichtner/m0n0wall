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
if ($_POST) {
    if ($_POST['vouchers']) {
        $test_results = voucher_auth($_POST['vouchers'], 1);
        echo "<p><table border=\"0\" cellspacing=\"0\" cellpadding=\"4\" width=\"100%\">\n";
        foreach ($test_results as $result) {
            if (strpos($result, " good ") || strpos($result, " granted ")) {
                echo "<tr><td bgcolor=\"#D9DEE8\"><img src=\"/pass.gif\"></td>";
                echo "<td bgcolor=\"#D9DEE8\">$result</td></tr>";
            } else {
                echo "<tr><td bgcolor=\"#FFD9D1\"><img src=\"/block.gif\"></td>";
                echo "<td bgcolor=\"#FFD9D1\">$result</td></tr>";
            }
        }
        echo "</table></p>";
    }
}
?>

<form action="status_captiveportal_test.php" method="post" enctype="multipart/form-data" name="iform" id="iform">
<table width="100%" border="0" cellpadding="0" cellspacing="0">
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

<table width="100%" border="0" cellpadding="6" cellspacing="0">
  <tr>
    <td valign="top" class="vncellreq">Voucher(s)</td>
    <td class="vtable">
    <textarea name="vouchers" cols="65" rows="3" type="text" id="vouchers" class="formpre"><?=htmlspecialchars($_POST['vouchers']);?></textarea>
    <br>
Enter multiple vouchers separated by space or newline. The remaining time, if valid, will be shown for each voucher.</td>      
  </tr>      
  <tr>
    <td width="22%" valign="top">&nbsp;</td>
    <td width="78%">
    <input name="Submit" type="submit" class="formbtn" value="Submit">
    </td>
  </tr>
</table>
</td></tr></table>
</form>
<p>
<?php include("fend.inc"); ?>
