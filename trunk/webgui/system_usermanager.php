#!/usr/local/bin/php
<?php 
/*
	$Id: system_usermanager.php
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

// The page title for non-admins
$pgtitle = array("System", "User password");
if ($_SERVER['REMOTE_USER'] === $config['system']['username']) {
	$pgtitle = array("System", "User manager");
}

?>
<?php include("fbegin.inc"); ?>
<?php 
if ($_SERVER['REMOTE_USER'] === $config['system']['username']) { 
	
	if ($_GET['act']=="new" || $_GET['act']=="edit") {
		if (isset($_GET['username'])) {
			$user=$config['system']['users'][$_GET['username']];
		}
	}	
	
	if (($_GET['act']=='delete') && (isset($_GET['username']))) {
		unset($config['system']['users'][$_GET['username']]);
		write_config();
		$retval = system_password_configure();
		$savemsg = get_std_save_message($retval);
		$savemsg="User ".$_GET['username']." successfully deleted<br>";		
	}
	
	if(isset($_POST['save'])) {
		//value-checking
		if(trim($_POST['password1'])!="********" && 
		   trim($_POST['password1'])!="" && 
		   trim($_POST['password1'])!=trim($_POST['password2'])){
		   	//passwords are to be changed but don't match
		   	$input_errors[]="passwords don't match";
		}
		if((trim($_POST['password1'])=="" || trim($_POST['password1'])=="********") && 
		   (trim($_POST['password2'])=="" || trim($_POST['password2'])=="********")){
		   	//assume password should be left as is if a password is set already.
			if(!empty($config['system']['users'][$_POST['old_username']]['password'])){
				$_POST['password1']="********";
				$_POST['password2']="********";
			} else {
				$input_errors[]="password must not be empty";
			}
		} else {
			if(trim($_POST['password1'])!=trim($_POST['password2'])){
			   	//passwords are to be changed or set but don't match
			   	$input_errors[]="passwords don't match";
			} else {
				//check password for invalid characters
				if(!preg_match('/^[a-zA-Z0-9_\-\.@\~\(\)\&\*\+§?!\$£°\%;:]*$/',$_POST['username'])){
					$input_errors[] = "password contains illegal characters, only  letters from A-Z and a-z, _, -, .,@,~,(,),&,*,+,§,?,!,$,£,°,%,;,: and numbers are allowed";
					//test pw: AZaz_-.@~()&*+§?!$£°%;:
				}
			}
		}
		if($_POST['username']==""){
			$input_errors[] = "username must not be empty!";
		}
		if($_POST['username']==$config['system']['username']) {
			$input_errors[] = "username can not match the administrator username!";
		}
		if($_POST['old_username'] != $_POST['username']) {
			// Either a new user, or one with a username change
			if (isset($config['system']['users'][$_POST['username']])) {
				$input_errors[] = "username can not match an existing user!";
			}
		}
		if(!isset($config['system']['groups'][$_POST['group']])) {
			$input_errors[] = "group does not exist, please define the group before assigning users.";
		}
		
		//check username: only allow letters from A-Z and a-z, _, -, . and numbers from 0-9 (note: username can
		//not contain characters which are not allowed in an xml-token. i.e. if you'd use @ in a username, config.xml
		//could not be parsed anymore!
		if(!preg_match('/^[a-zA-Z0-9_\-\.]*$/',$_POST['username'])){
			$input_errors[] = "username contains illegal characters, only letters from A-Z and a-z, _, -, . and numbers are allowed";
		}
		if(!empty($input_errors)){
			//there are illegal inputs --> print out error message and show formula again 
			//and fill in all recently entered values except passwords
			$_GET['act']="new";
			$_POST['old_username']=($_POST['old_username'] ? $_POST['old_username'] : $_POST['username']);
			$_GET['username']=$_POST['old_username'];

			$user['fullname']=$_POST['fullname'];

		} else {
			//all values are okay --> saving changes
			$_POST['username']=trim($_POST['username']);
			if($_POST['old_username']!="" && $_POST['old_username']!=$_POST['username']){
				//change the username (which is used as array-index)
				$config['system']['users'][$_POST['username']]=$config['system']['users'][$_POST['old_username']];
				unset($config['system']['users'][$_POST['old_username']]);
			}
			$config['system']['users'][$_POST['username']]['fullname']=trim($_POST['fullname']);
			if(trim($_POST['password1'])!="********" && trim($_POST['password1'])!=""){
				$config['system']['users'][$_POST['username']]['password']=crypt(trim($_POST['password1']));
			}
			$config['system']['users'][$_POST['username']]['group']=trim($_POST['group']);
			// Remove config information from old way of handling sub-admin users.
			if (isset($config['system']['users'][$_POST['username']]['pages'])) 
			  unset($config['system']['users'][$_POST['username']]['pages']);
			write_config();
			$retval = system_password_configure();
			$savemsg = get_std_save_message($retval);
			$savemsg="User ".$_POST['username']." successfully saved<br>";
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
	if($_GET['act']=="edit" && isset($_GET['username'])){
		$user=$config['system']['users'][$_GET['username']];
	}
?>
	<form action="system_usermanager.php" method="post" name="iform" id="iform">
              <table width="100%" border="0" cellpadding="6" cellspacing="0">
                <tr> 
                  <td width="22%" valign="top" class="vncellreq">Username</td>
                  <td width="78%" class="vtable"> 
                    <input name="username" type="text" class="formfld" id="username" size="20" value="<?=$_GET['username'];?>"> 
                    </td>
                </tr>
                <tr> 
                  <td width="22%" valign="top" class="vncellreq">Password</td>
                  <td width="78%" class="vtable"> 
                    <input name="password1" type="password" class="formfld" id="password1" size="20" value="<?php echo ($_GET['act']=='edit' ? "********" : "" ); ?>"> <br>
					<input name="password2" type="password" class="formfld" id="password2" size="20" value="<?php echo ($_GET['act']=='edit' ? "********" : "" ); ?>">
&nbsp;(confirmation)					</td>
                </tr>
                <tr> 
                  <td width="22%" valign="top" class="vncell">Full name</td>
                  <td width="78%" class="vtable"> 
                    <input name="fullname" type="text" class="formfld" id="fullname" size="20" value="<?=htmlspecialchars($user['fullname']);?>">
                    <br>
                    User's full name, for your own information only</td>
                </tr>
                <tr> 
                  <td width="22%" valign="top" class="vncell">Group Name</td>
                  <td width="78%" class="vtable">
				  <select name="group" class="formfld" id="group">
                      <?php foreach ($config['system']['groups'] as $gname => $group): ?>
                      	
                      <option value="<?=$gname;?>" <?php if ($gname == $user['group']) echo "selected"; ?>>
                      <?=htmlspecialchars($gname);?>
                      </option>
                      <?php endforeach; ?>
                    </select>                   
                    <br>
                    The admin group to which this user is assigned.</td>
                </tr>                
                <tr> 
                  <td width="22%" valign="top">&nbsp;</td>
                  <td width="78%"> 
                    <input name="save" type="submit" class="formbtn" value="Save"> 
                    <input name="old_username" type="hidden" value="<?=$_GET['username'];?>">
                  </td>
                </tr>
              </table>
     </form>
<?php
} else {
?>
     <table width="100%" border="0" cellpadding="0" cellspacing="0">
        <tr>
           <td width="35%" class="listhdrr">Username</td>
           <td width="20%" class="listhdrr">Full name</td>
           <td width="20%" class="listhdrr">Group</td>                  
           <td width="10%" class="list"></td>
		</tr>
<?php
	if(is_array($config['system']['users'])){
		foreach($config['system']['users'] as $username => $user){
?>
		<tr>
           <td class="listlr">
              <?=$username; ?>&nbsp;
           </td>
           <td class="listr">
              <?=htmlspecialchars($user['fullname']);?>&nbsp;
           </td>
              <td class="listr">
              <?=$user['group'];?>
              </td>
           <td valign="middle" nowrap class="list"> <a href="system_usermanager.php?act=edit&username=<?=$username; ?>"><img src="e.gif" title="edit user" width="17" height="17" border="0"></a>
              &nbsp;<a href="system_usermanager.php?act=delete&username=<?=$username; ?>" onclick="return confirm('Do you really want to delete this User?')"><img src="x.gif" title="delete user" width="17" height="17" border="0"></a></td>
		</tr>
<?php
		}
	} ?>
	    <tr> 
			<td class="list" colspan="3"></td>
			<td class="list"> <a href="system_usermanager.php?act=new"><img src="plus.gif" title="add user" width="17" height="17" border="0"></a></td>
		</tr>
		<tr>
			<td colspan="3">
		      Additional webGui users can be added here.  User permissions are determined by the admin group they are a member of.
			</td>
		</tr>
 </table>
<?php } ?>
     
  </td>
  </tr>
  </table>
<?php 
} else { // end of admin user code, start of normal user code
	if(isset($_POST['save'])) {
		//value-checking
		if(trim($_POST['password1'])!="********" && 
		   trim($_POST['password1'])!="" && 
		   trim($_POST['password1'])!=trim($_POST['password2'])){
		   	//passwords are to be changed but don't match
		   	$input_errors[]="passwords don't match";
		}
		if((trim($_POST['password1'])=="" || trim($_POST['password1'])=="********") && 
		   (trim($_POST['password2'])=="" || trim($_POST['password2'])=="********")){
		   	//assume password should be left as is if a password is set already.
			if(!empty($config['system']['users'][$_POST['old_username']]['password'])){
				$_POST['password1']="********";
				$_POST['password2']="********";
			} else {
				$input_errors[]="password must not be empty";
			}
		} else {
			if(trim($_POST['password1'])!=trim($_POST['password2'])){
			   	//passwords are to be changed or set but don't match
			   	$input_errors[]="passwords don't match";
			} else {
				//check password for invalid characters
				if(!preg_match('/^[a-zA-Z0-9_\-\.@\~\(\)\&\*\+§?!\$£°\%;:]*$/',$_POST['username'])){
					$input_errors[] = "password contains illegal characters, only  letters from A-Z and a-z, _, -, .,@,~,(,),&,*,+,§,?,!,$,£,°,%,;,: and numbers are allowed";
					//test pw: AZaz_-.@~()&*+§?!$£°%;:
				}
			}
		}
		if (!$input_errors) {
			//all values are okay --> saving changes
			if(trim($_POST['password1'])!="********" && trim($_POST['password1'])!=""){
				$config['system']['users'][$_SERVER['REMOTE_USER']]['password']=crypt(trim($_POST['password1']));
			}
			write_config();
			$retval = system_password_configure();
			$savemsg = get_std_save_message($retval);
			$savemsg = "Password successfully changed<br>";
		}		
	}

	
?>
<?php if ($input_errors) print_input_errors($input_errors); ?>
<?php if ($savemsg) print_info_box($savemsg); ?>
      <form action="system_usermanager.php" method="post" name="iform" id="iform">
         <table width="100%" border="0" cellpadding="6" cellspacing="0">
            <tr> 
              <td colspan="2" valign="top" class="listtopic"><?=$_SERVER['REMOTE_USER']?>'s Password</td>
            </tr>
		    <tr> 
		      <td width="22%" valign="top" class="vncell">Password</td>
		      <td width="78%" class="vtable"> <input name="password1" type="password" class="formfld" id="password1" size="20"> 
		        <br> <input name="password2" type="password" class="formfld" id="password2" size="20"> 
		        &nbsp;(confirmation) <br> <span class="vexpl">Select a new password</span></td>
		    </tr>
            <tr> 
              <td width="22%" valign="top">&nbsp;</td>
              <td width="78%"> 
                <input name="save" type="submit" class="formbtn" value="Save"> 
              </td>
            </tr>		    
         </table>
      </form>		    

<?php 
} // end of normal user code ?>
<?php include("fend.inc"); ?>
