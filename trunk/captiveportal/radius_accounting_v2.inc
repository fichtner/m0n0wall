<?php
/* vim: set expandtab tabstop=4 shiftwidth=4: */
/*

$Id$

Copyright (c) 2006, Jonathan De Graeve <jonathan.de.graeve@imelda.be>
All rights reserved.

Redistribution and use in source and binary forms, with or without 
modification, are permitted provided that the following conditions 
are met:

1. Redistributions of source code must retain the above copyright 
   notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright 
   notice, this list of conditions and the following disclaimer in the 
   documentation and/or other materials provided with the distribution.
3. The names of the authors may not be used to endorse or promote products 
   derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND 
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED 
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. 
IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, 
INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, 
BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, 
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY 
OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING 
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, 
EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

This code cannot simply be copied and put under the GNU Public License or 
any other GPL-like (LGPL, GPL2) License.

    This code is made possible thx to samples made by Michael Bretterklieber <michael@bretterklieber.com>
    author of the PHP PECL Radius package

    Changes made include:
    * Porting to M0n0wall environment
    * Sends NAS-IP-Address AND Nas-Identifier (first word of hostname) in Accounting request
    * Sending of Framed-IP-Address (client IP)
    * Sending of Called-Station-ID (NAS IP)
    * Sending of Calling-Station-ID (client IP)
    * RADIUS Extensions (RFC2869) => Acct-(Input|Output)-Gigawords
    * TODO implementation of different Acct-Terminate-Cause (Disconnect reason) (should be changed in index.php)

*/

/* 
RADIUS ACCOUNTING START 
-----------------------
*/

function RADIUS_ACCOUNTING_START($username,$sessionid,$radiusip,$radiusport,$radiuskey,$clientip) {
    global $debug;

$racct = new Auth_RADIUS_Acct_Start;
$racct->addServer($radiusip, $radiusport, $radiuskey);
/* Different Authentication options
 RADIUS_AUTH_RADIUS => authenticated via Radius
 RADIUS_AUTH_LOCAL => authenicated local
 RADIUS_AUTH_REMOTE => authenticated remote
*/

$racct->authentic = RADIUS_AUTH_RADIUS;
$racct->username = $username;
$status = $racct->start();
if(PEAR::isError($status)) {
    if ($debug)
    printf("Radius start: %s<br>\n", $status->getMessage());
    exit;
}

/*
NAS_PORT_TYPE, int => RADIUS_ETHERNET (15), RADIUS_WIRELESS_OTHER (18), RADIUS_WIRELESS_IEEE_802_11 (19)
*/


// Other options
$racct->putAttribute(RADIUS_SERVICE_TYPE, RADIUS_LOGIN);
$racct->putAttribute(RADIUS_NAS_PORT, 0); 
$racct->putAttribute(RADIUS_NAS_PORT_TYPE, RADIUS_ETHERNET);
$racct->putAttribute(RADIUS_ACCT_SESSION_ID, $sessionid); 
$racct->putAttribute(RADIUS_FRAMED_IP_ADDRESS, $clientip); 
$result = $racct->send();
if (PEAR::isError($result)) {
    $retvalue = 1;   
if ($debug)
    printf("Radius send failed: %s<br>\n", $result->getMessage());
} else if ($result === true) {
    $retvalue = 5 ;
    if ($debug)
    printf("Radius Accounting succeeded<br>\n") ;
} else {
    $retvalue = 1 ;
    if ($debug)
    printf("Radius Accounting rejected<br>\n") ;
}

$racct->close();

return $retvalue ;

}

/*
RADIUS ACCOUNTING STOP/UPDATE
-----------------------------
*/

p
function RADIUS_ACCOUNTING_STOP($ruleno,$username,$sessionid,$start_time,$radiusip,$radiusport,$radiuskey,$clientip,$interimupdate=false,$radius_term_cause = "RADIUS_TERM_IDLE_TIMEOUT") {
    global $debug;

    exec("/bin/hostname", $nasHostname) ;
    if(!$nasHostname[0])
            $nasHostname[0] = "quewall" ;

    $input_pkts = $input_bytes = $input_gigawords = $output_pkts = $output_bytes = $output_gigawords = 0 ;

    exec("/sbin/ipfw show {$ruleno}", $ipfw) ;
    preg_match("/(\d+)\s+(\d+)\s+(\d+)\s+skipto/", $ipfw[0], $matches) ;
    $input_pkts = $matches[2] ;
    $input_bytes = remainder($matches[3]); 
    $input_gigawords = gigawords($matches[3]);

    unset($matches) ;
    preg_match("/(\d+)\s+(\d+)\s+(\d+)\s+skipto/", $ipfw[1], $matches) ;
    $output_pkts = $matches[2] ;
    $output_bytes = remainder($matches[3]);
    $output_gigawords = gigawords($matches[3]);


    $nas_ip = get_current_wan_address();
    $nas_ip_exp = explode(".",$nas_ip);
    $nas_port = $ruleno - 10000;
    $nas_mac = get_interface_mac($config['interfaces']['wan']['if']);
    $ip_exp=explode(".",$clientip);
    $session_time = $stop_time - $start_time;
    $radiusvendor = $config['captiveportal']['radiusvendor'] ? $config['captiveportal']['radiusvendor'] : null

    if ($debug)
        echo "<br>radius-port: $radiusport<br>radius-host: $radiusip<br>username: $username<hr>\n";


    // See if we should use Accounting Interim Updates or Accounting STOP messages
    if ($interimupdate)
        $racct = new_Auth_RADIUS_Acct_Update;
    else
        $racct = new_Auth_RADIUS_Acct_Stop; 

// Construct data package
$racct->addServer($radiusip, $radiusport, $radiuskey);
$racct->username = $username;


$status = $racct->start();
if(PEAR::isError($status)) {
    if ($debug)
    printf("Radius start: %s<br>\n", $status->getMessage());
    exit;
}
// you can put any additional attributes here
//$racct->putAttribute(RADIUS_SERVICE_TYPE, RADIUS_LOGIN);

// Do the accounting style as configured in GUI
	switch($radiusvendor) {

        case 'cisco':
        $rauth->putAttribute(RADIUS_CALLED_STATION_ID, $clientmac);
        $rauth->putAttribute(RADIUS_CALLING_STATION_ID, $clientip);
        break;

        default:
        $rauth->putAttribute(RADIUS_CALLED_STATION_ID, $nas_mac);
        $rauth->putAttribute(RADIUS_CALLING_STATION_ID, $clientmac);
	}

	// Default attributes
	$rauth->putAttribute(RADIUS_NAS_PORT, $nas_port)

$session_time = $stop_time - $start_time;
// We have 2 ways to set the session-time, we will see which one to use in a later version
$racct->session_time = $session_time;
$racct->putAttribute(RADIUS_ACCT_SESSION_TIME, $session_time);

// Set the session_id here since we removed it into the radius.inc removing the possibility to overwrite it by $racct->session_id
$racct->putAttribute(RADIUS_ACCT_SESSION_ID, $sessionid);

$racct->putAttribute(RADIUS_FRAMED_IP_ADDRESS, $clientip);
$racct->putAttribute(RADIUS_CALLING_STATION_ID, $clientip);
$racct->putAttribute(RADIUS_CALLED_STATION_ID, $nas_ip_address);
$racct->putAttribute(RADIUS_ACCT_INPUT_PACKETS, $input_pkts);
$racct->putAttribute(RADIUS_ACCT_INPUT_OCTETS, $input_bytes);
$racct->putAttribute(RADIUS_ACCT_INPUT_GIGAWORDS, $input_gigawords);
$racct->putAttribute(RADIUS_ACCT_OUTPUT_PACKETS, $output_pkts);
$racct->putAttribute(RADIUS_ACCT_OUTPUT_OCTETS, $output_bytes);
$racct->putAttribute(RADIUS_ACCT_OUTPUT_GIGAWORDS, $output_gigawords);
if (!$interimupdate)
$racct->putAttribute(RADIUS_ACCT_TERMINATE_CAUSE, $radius_term_cause);

$result = $racct->send();
if (PEAR::isError($result)) {
    $retvalue = 1;
    if ($debug)
    printf("Radius send failed: %s<br>\n", $result->getMessage());
} else if ($result === true) {
    $retvalue = 5 ;
    if ($debug)
    printf("Radius Accounting succeeded<br>\n");
} else {
    $retvalue = 1 ;
    if ($debug)
    printf("Radius Accounting rejected<br>\n");
}

$racct->close();

return $retvalue ;
// 5 -> Accounting-Response
// See RFC2866 for this.

}

function get_nas_ip() {
        global $config;
                        
        /* static WAN IP address */
        return $config['interfaces']['wan']['ipaddr'];   
}

function gigawords($bytes) {
        /* We use BCMath functions since normal integers don't work */
        $gigawords = bcdiv( bcsub( $bytes, remainder($bytes) ) , 4294967295);
    
        return $gigawords;
}

function remainder($bytes) {
        /* Calculate the remainder */
        $bytes = bcmod($bytes, 4294967295);

        return $bytes;
}

        

?>
