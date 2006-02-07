#!/usr/local/bin/php
<?php 
/*
	$Id: system_groupmanager.php 
	part of m0n0wall (http://m0n0.ch/wall)

	Copyright (C) 2005 Paul Taylor <paultaylor@winn-dixie.com>.
	All rights reserved. 

	Copyright (C) 2003-2005 Manuel Kasper <mk@neon1.net>.
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

require("guiconfig.inc");

$pgtitle = array("System", "Group manager");

// Returns an array of pages with their descriptions
function getAdminPageList() {
	global $g;
	
    $tmp = Array();

    if ($dir = opendir($g['www_path'])) {
		while($file = readdir($dir)) {
	    	// Make sure the file exists
	    	if($file != "." && $file != ".." && $file[0] != '.') {
	    		// Is this a .php file?
	    		if (fnmatch('*.php',$file)) {
	    			// Read the description out of the file
		    		$contents = file_get_contents($file);
		    		// Looking for a line like:
		    		// $pgtitle = array("System", "Group manager");
		    		$offset = strpos($contents,'$pgtitle');
		    		$titlepos = strpos($contents,'(',$offset);
		    		$titleendpos = strpos($contents,')',$titlepos);
		    		if (($offset > 0) && ($titlepos > 0) && ($titleendpos > 0)) {
		    			// Title found, extract it
		    			$title = str_replace(',',':',str_replace(array('"'),'',substr($contents,++$titlepos,($titleendpos - $titlepos))));
		    			$tmp[$file] = trim($title);
		    		}
		    		else {
		    			$tmp[$file] = '';
		    		}
	    		
	    		}
	        }
		}

        closedir($dir);
        
        // Sets Interfaces:Optional page that didn't read in properly with the above method,
        // and pages that don't have descriptions.
        $tmp['interfaces_opt.php'] = "Interfaces: Optional";
        $tmp['graph.php'] = "Diagnostics: Interface Traffic";
        $tmp['graph_cpu.php'] = "Diagnostics: CPU Utilization";
        $tmp['exec.php'] = "Hidden: Exec";
        $tmp['exec_raw.php'] = "Hidden: Exec Raw";
        $tmp['status.php'] = "Hidden: Detailed Status";
        $tmp['uploadconfig.php'] = "Hidden: Upload Configuration";
        $tmp['index.php'] = "*Landing Page after Login";
        $tmp['system_usermanager.php'] = "*User Password";
        $tmp['diag_logs_settings.php'] = "Diagnostics: Logs: Settings";
        $tmp['diag_logs_vpn.php'] = "Diagnostics: Logs: PPTP VPN";
        $tmp['diag_logs_filter.php'] = "Diagnostics: Logs: Firewall";
        $tmp['diag_logs_portal.php'] = "Diagnostics: Logs: Captive Portal";
        $tmp['diag_logs_dhcp.php'] = "Diagnostics: Logs: DHCP";
        $tmp['diag_logs.php'] = "Diagnostics: Logs: System";
        

        asort($tmp);
        return $tmp;
    }
}

?>
<?php include("fbegin.inc"); ?>

<?php 
// Get a list of all admin pages & Descriptions
$pages = getAdminPageList();

if ($_GET['act']=="new" || $_GET['act']=="edit") {
	if (isset($_GET['groupname'])) {
		$group=$config['system']['groups'][$_GET['groupname']];
	}
}	

if (($_GET['act']=='delete') && (isset($_GET['groupname']))) {

	// See if there are any users who are members of this group. 
	$ok_to_delete = true;
	if (is_array($config['system']['users'])) {
		foreach ($config['system']['users'] as $key => $user) {
			if ($user['group'] == $_GET['groupname']) {
				$ok_to_delete = false;
				$input_errors[] = "users still exist who are members of this group!";
				break;
			}
		}
	}
	
	if ($ok_to_delete) {
		unset($config['system']['groups'][$_GET['groupname']]);
		write_config();
		$retval = system_password_configure();
		$savemsg = get_std_save_message($retval);
		$savemsg="Group ".$_GET['groupname']." successfully deleted<br>";		
	}
}

if(isset($_POST['save'])) {
	//value-checking
	if($_POST['groupname']==""){
		$input_errors[] = "group name must not be empty!";
	}
	if($_POST['old_groupname'] != $_POST['groupname']) {
		// Either a new group, or one with a group name change
		if (isset($config['system']['groups'][$_POST['groupname']])) {
			$input_errors[] = "group name can not match an existing group!";
		}
	}
	
	//check groupname: only allow letters from A-Z and a-z, _, -, . and numbers from 0-9 (note: groupname can
	//not contain characters which are not allowed in an xml-token. i.e. if you'd use @ in a groupname, config.xml
	//could not be parsed anymore!
	if(!preg_match('/^[a-zA-Z0-9_\-\.]*$/',$_POST['groupname'])){
		$input_errors[] = "groupname contains illegal characters, only letters from A-Z and a-z, _, -, . and numbers are allowed";
	}
	if(!empty($input_errors)){
		//there are illegal inputs --> print out error message and show formula again 
		//and fill in all recently entered values except passwords
		$_GET['act']="new";
		$_POST['old_groupname']=($_POST['old_groupname'] ? $_POST['old_groupname'] : $_POST['groupname']);
		$_GET['groupname']=$_POST['old_groupname'];

		$group['description']=$_POST['description'];

		foreach ($pages as $fname => $title) {
			$id = str_replace('.php','',$fname);
			if ($_POST[$id] == 'yes') {
				$group['pages'][] = $fname;
			}			
		}
		
	} else {
		//all values are okay --> saving changes
		$_POST['groupname']=trim($_POST['groupname']);
		if($_POST['old_groupname']!="" && $_POST['old_groupname']!=$_POST['groupname']){
			//change the groupname (which is used as array-index)
			$config['system']['groups'][$_POST['groupname']]=$config['system']['groups'][$_POST['old_groupname']];
			unset($config['system']['groups'][$_POST['old_groupname']]);

			// Group name was changed.  Update all users that are members of this group to point to the new groupname.
			foreach ($config['system']['users'] as $key => $user) {
				if ($user['group'] == $_POST['old_groupname']) 
					$config['system']['users'][$key]['group'] = $_POST['groupname'];				
			}
		}
		$config['system']['groups'][$_POST['groupname']]['description']=trim($_POST['description']);
		// Clear pages info and read pages from POST
		if (isset($config['system']['groups'][$_POST['groupname']]['pages']))
			unset($config['system']['groups'][$_POST['groupname']]['pages']);
		foreach ($pages as $fname => $title) {
			$id = str_replace('.php','',$fname);
			if ($_POST[$id] == 'yes') {
				$config['system']['groups'][$_POST['groupname']]['pages'][] = $fname;
			}
		}
		write_config();
		$retval = system_password_configure();
		$savemsg = get_std_save_message($retval);
		$savemsg="Group ".$_POST['groupname']." successfully saved<br>";
	}
}

?>
<?php if ($input_errors) print_input_errors($input_errors); ?>
<?php if ($savemsg) print_info_box($savemsg); ?>
<table width="100%" border="0" cellpadding="0" cellspacing="0">
  <tr><td class="tabnavtbl">
  <ul id="tabnav">
	<?php 
    	$tabs = array('Users' => 'system_usermanager.php',
            		  'Groups' => 'system_groupmanager.php');
		dynamic_tab_menu($tabs);
    ?>     
  </ul>
  </td></tr>    
<tr>
  <td class="tabcont">
<?php
if($_GET['act']=="new" || $_GET['act']=="edit"){
	if($_GET['act']=="edit" && isset($_GET['groupname'])){
		$group=$config['system']['groups'][$_GET['groupname']];
	}
?>
<form action="system_groupmanager.php" method="post" name="iform" id="iform">
          <table width="100%" border="0" cellpadding="6" cellspacing="0">
            <tr> 
              <td width="22%" valign="top" class="vncellreq">Group name</td>
              <td width="78%" class="vtable"> 
                <input name="groupname" type="text" class="formfld" id="groupname" size="20" value="<?=$_GET['groupname'];?>"> 
                </td>
            </tr>
            <tr> 
              <td width="22%" valign="top" class="vncell">Description</td>
              <td width="78%" class="vtable"> 
                <input name="description" type="text" class="formfld" id="description" size="20" value="<?=htmlspecialchars($group['description']);?>">
                <br>
                Group description, for your own information only</td>
            </tr>
            <tr>
			  	<td colspan="4"><br>&nbsp;Select that pages that this group may access.  Members of this group will be able to perform all actions that<br>&nbsp; are possible from each individual web page.  Ensure you set access levels appropriately.<br><br>
			  	<span class="vexpl"><span class="red"><strong>&nbsp;Note: </strong></span>Pages 
          marked with an * are strongly recommended for every group.</span>
			  	</td>
				</tr>
            <tr>
              <td colspan="2">
              <table width="100%" border="0" cellpadding="0" cellspacing="0">
              <tr>
                <td class="listhdrr">&nbsp;</td>
                <td class="listhdrr">Page Description</td>
                <td class="listhdr">Filename</td>
              </tr>
              <?php 
              foreach ($pages as $fname => $title) {
              	$id = str_replace('.php','',$fname);
              	?>
              	<tr><td class="listlr">
              	<input name="<?=$id?>" type="checkbox" id="<?=$id?>" value="yes" <?php if (in_array($fname,$group['pages'])) echo "checked"; ?>></td>
              	<td class="listr"><?=$title?></td>
              	<td class="listr"><?=$fname?></td>
              	</tr>
              	<?
              } ?>
              </table>
              </td>
            </tr>
            <tr> 
              <td width="22%" valign="top">&nbsp;</td>
              <td width="78%"> 
                <input name="save" type="submit" class="formbtn" value="Save"> 
                <input name="old_groupname" type="hidden" value="<?=$_GET['groupname'];?>">
              </td>
            </tr>
          </table>
 </form>
<?php
} else {
?>
 <table width="100%" border="0" cellpadding="0" cellspacing="0">
    <tr>
       <td width="35%" class="listhdrr">Group name</td>
       <td width="20%" class="listhdrr">Description</td>
       <td width="20%" class="listhdrr">Pages Accessible</td>                  
       <td width="10%" class="list"></td>
	</tr>
<?php
	if(is_array($config['system']['groups'])){
		foreach($config['system']['groups'] as $groupname => $group){
?>
		<tr>
           <td class="listlr">
              <?=$groupname; ?>&nbsp;
           </td>
           <td class="listr">
              <?=htmlspecialchars($group['description']);?>&nbsp;
           </td>
              <td class="listr">
              <?=count($group['pages']);?>
              </td>
           <td valign="middle" nowrap class="list"> <a href="system_groupmanager.php?act=edit&groupname=<?=$groupname; ?>"><img src="e.gif" title="edit group" width="17" height="17" border="0"></a>
              &nbsp;<a href="system_groupmanager.php?act=delete&groupname=<?=$groupname; ?>" onclick="return confirm('Do you really want to delete this Group?')"><img src="x.gif" title="delete group" width="17" height="17" border="0"></a></td>
		</tr>
<?php
		}
	} ?>
	    <tr> 
			<td class="list" colspan="3"></td>
			<td class="list"> <a href="system_groupmanager.php?act=new"><img src="plus.gif" title="add group" width="17" height="17" border="0"></a></td>
		</tr>
		<tr>
			<td colspan="3">
		      Additional webGui admin groups can be added here.  Each group can be restricted to specific portions of the webGUI.  Individually select the desired web pages each group may access.  For example, a troubleshooting group could be created which has access only to selected Status and Diagnostics pages.
			</td>
		</tr>
 </table>
<?php } ?>
     
  </td>
  </tr>
  </table>
<?php include("fend.inc"); ?>