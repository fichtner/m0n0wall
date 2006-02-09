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

*/

/*
RADIUS ACCOUNTING START 
-----------------------
*/

function RADIUS_ACCOUNTING_START($ruleno,$username,$sessionid,$radiusip,$radiusport,$radiuskey,$clientip,$clientmac) {

    global $config;

    $nas_mac = get_interface_mac($config['interfaces']['wan']['if']); // This function is defined in radius_authentication.inc
    $nas_port = $ruleno - 10000;
    $ip_exp=explode(".",$clientip);
    $radiusvendor = $config['captiveportal']['radiusvendor'] ? $config['captiveportal']['radiusvendor'] : null;

    switch($radiusvendor) {

        case 'cisco':
        $calledstationid = $clientmac;
        $callingstationid = $clientip;
        break;

        default:
        $calledstationid = $nas_mac;
        $callingstationid = $clientmac;
    }

    if ($debug)
        echo "<br>radius-port: $radiusport<br>radius-host: $radiusip<br>username: $username<hr>\n";

// Create our instance
$racct = new Auth_RADIUS_Acct_Start;

/* Different Authentication options
 *
 * Its possible todo other authentication methods but still do radius accounting

 RADIUS_AUTH_RADIUS => authenticated via Radius
 RADIUS_AUTH_LOCAL => authenicated local
 RADIUS_AUTH_REMOTE => authenticated remote

 */

$racct->authentic = RADIUS_AUTH_RADIUS;

// Construct data package
$racct->addServer($radiusip, $radiusport, $radiuskey);
$racct->username = $username;

if (PEAR::isError($racct->start())) {
    $retvalue['acct_val'] = 1;
    $retvalue['error'] = $rauth->getMessage();
    if ($debug)
        printf("Radius start: %s<br>\n", $retvalue['error']);

    /* Old code:
     $status = $racct->start();
     if(PEAR::isError($status)) {
         if ($debug)
             printf("Radius start: %s<br>\n", $status->getMessage());
             exit;
     }
     */
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
    // Send request

    $result = $racct->send();
    if (PEAR::isError($result)) {
        $retvalue['acct_val'] = 1;
        $retvalue['error'] = $result->getMessage();
        if ($debug)
            printf("Radius send failed: %s<br>\n", $retvalue['error']);
    } else if ($result === true) {
        $retvalue['acct_val'] = 5 ;
        if ($debug)
            printf("Radius Accounting succeeded<br>\n");
    } else {
        $retvalue['acct_val'] = 1 ;
        if ($debug)
            printf("Radius Accounting rejected<br>\n");
    }

    // close OO RADIUS_ACCOUNTING
    $racct->close();

    return $retvalue ;
    // 5 -> Accounting-Response
    // See RFC2866 for this.

}

/*
RADIUS ACCOUNTING STOP/UPDATE
-----------------------------
*/

function RADIUS_ACCOUNTING_STOP($ruleno,$username,$sessionid,$start_time,$radiusip,$radiusport,$radiuskey,$clientip,$clientmac, $term_cause = 1, $interimupdate=false,$stop_time = null) {
    global $config;

    $stop_time = (empty($stop_time)) ? time() : $stop_time;

    $nas_port = $ruleno - 10000;
    $nas_mac = get_interface_mac($config['interfaces']['wan']['if']);
    $session_time = $stop_time - $start_time;
    $radiusvendor = $config['captiveportal']['radiusvendor'] ? $config['captiveportal']['radiusvendor'] : null

    if ($debug)
        echo "<br>radius-port: $radiusport<br>radius-host: $radiusip<br>username: $username<hr>\n";


    // See if we should use Accounting Interim Updates or Accounting STOP messages
    if ($interimupdate)
        $racct = new_Auth_RADIUS_Acct_Update;
    else
        $racct = new_Auth_RADIUS_Acct_Stop;

/*
 * Currently disabled
Add support for more then one radiusserver. 
At most 10 servers may be specified. 
When multiple servers are given, they are tried in round-robin fashion until a valid response is received 

foreach ($radiusservers as $radsrv) {

    // Add a new server to our instance
    $rauth->addServer($radsrv['ipaddr'], $radsrv['port'], $radsrv['key']);

}
*/

$racct->authentic = RADIUS_AUTH_RADIUS;

// Construct data package
$racct->addServer($radiusip, $radiusport, $radiuskey);
$racct->username = $username;

if (PEAR::isError($racct->start())) {
    $retvalue['acct_val'] = 1;
    $retvalue['error'] = $rauth->getMessage();
    if ($debug)
        printf("Radius start: %s<br>\n", $retvalue['error']);
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

    // Send request

    $result = $racct->send();
    if (PEAR::isError($result)) {
        $retvalue['acct_val'] = 1;
        $retvalue['error'] = $result->getMessage();
        if ($debug)
            printf("Radius send failed: %s<br>\n", $retvalue['error']);
    } else if ($result === true) {
        $retvalue['acct_val'] = 5 ;
        if ($debug)
            printf("Radius Accounting succeeded<br>\n");
    } else {
        $retvalue['acct_val'] = 1 ;
        if ($debug)
            printf("Radius Accounting rejected<br>\n");
    }

    // close OO RADIUS_ACCOUNTING
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

/**
 * This function will calculate the traffic produced by a host
 * based on its firewall rule
 *
 * Will be moved to in captiveportal.inc
 * so that we can implement user volume limits
 *
 * Point of view: Client
 *
 */

function volumeCalculation($ruleno) {

    $volume = array();

    /* Ingress */
    exec("/sbin/ipfw show {$ruleno}", $ipfw);
    preg_match("/(\d+)\s+(\d+)\s+(\d+)\s+skipto/", $ipfw[0], $matches);
    $volume['input_pkts'] = $matches[2];
    $volume['input_bytes'] = $matches[3];
    /* These functions will be moved to the accounting part
    $volume['input_bytes_radius'] = remainder($matches[3]);
    $volume['input_gigawords'] = gigawords($matches[3]);
    */

    /* Flush internal buffer */
    unset($matches);

    /* Outgress */
    preg_match("/(\d+)\s+(\d+)\s+(\d+)\s+skipto/", $ipfw[1], $matches);
    $volume['output_pkts'] = $matches[2];
    $volume['output_bytes'] = $matches[3];
    /* These functions will be moved to the accounting part
    $volume['output_bytes_radius'] = remainder($matches[3]);
    $volume['output_gigawords'] = gigawords($matches[3]);
    */

    return $volume;
}

/**
 * Radius Volume Helpers
 *
 */

function gigawords($bytes) {

    /* We use BCMath functions since normal integers don't work with so large numbers */
    $gigawords = bcdiv( bcsub( $bytes, remainder($bytes) ) , 2147483647) ;

    return $gigawords;
}

function remainder($bytes) {

    /* Calculate the bytes we are going to send to the radius. */
    $bytes = bcmod($bytes, 2147483647);

    return $bytes;
}

?>
