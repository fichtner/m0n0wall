<?php
/*

    $Id$

    radius_accounting.inc
    part of m0n0wall (http://m0n0.ch/wall)
    
    Copyright (C) M0n0wall Project
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

function RADIUS_ACCOUNTING_START($ruleno,$username,$sessionid,$radiusip,$radiusport,$radiuskey,$clientip,$clientmac) {
    # $debug = 1 ;
    global $config;

    exec("/bin/hostname", $nasHostname) ;
    if(!$nasHostname[0])
        $nasHostname[0] = "m0n0wall" ;

    $fd = @fsockopen("udp://$radiusip",$radiusport,$errno,$errstr,3) ;
    if(!$fd) 
        return 1 ; /* error return */

    /* set 5 second timeout on socket i/o */
    stream_set_timeout($fd, 5) ;

    $nas_ip = get_current_wan_address();
    $nas_ip_exp = explode(".",$nas_ip);
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

    /* Initialise rand function, make it more random */
    srand((double)microtime() * 1000000);

    $thisidentifier=rand()%256;

    $length=4+                          // header
        16+                             // auth code
        6+                              // service type
        2+strlen($username)+            // username
        2+strlen($nasHostname[0])+      // nasIdentifier
        6+                              // nasPort
        6+                              // nasPortType
        6+                              // Acct Status Type
        6+                              // Acct RADIUS Authenticated
        2+strlen($sessionid)+           // Acct SessionID
        2+strlen($calledstationid)+     // Called-Station-ID
        2+strlen($callingstationid)+    // Calling-Station-ID
        6+                              // nas-IP-Address
        6;                              // Framed-IP-Address

    //          v   v   v     v   v   v     v     v     v     1   v
    // Line #   1   2   3     4   5   6     7     8     9     0   E
    $data=pack("CCCCNNNNCCCCCCCCa*CCa*CCCCCCCCCCCCCCCCCCCCCCCCCCa*CCa*CCa*CCCCCCCCCCCC",
        4,$thisidentifier,$length/256,$length%256,      // header
        0,0,0,0,                                        // authcode
        6,6,0,0,0,1,                                    // service type
        1,2+strlen($username),$username,                // username
        32,2+strlen($nasHostname[0]),$nasHostname[0],   // nasIdentifier
        5,6,0,0,0,$nas_port,                            // nasPort
        61,6,0,0,0,15,                                  // nasPortType = Ethernet
        40,6,0,0,0,1,                                   // Acct Status Type = Start
        45,6,0,0,0,1,                                   // Acct RADIUS Authenticated
        44,2+strlen($sessionid),$sessionid,             // Acct Session ID
        30,2+strlen($calledstationid),$calledstationid, // Called-Station-ID
        31,2+strlen($callingstationid),$callingstationid,                 // Calling-Station-ID
        4,6,$nas_ip_exp[0],$nas_ip_exp[1],$nas_ip_exp[2],$nas_ip_exp[3],  // nas-IP-Address
        8,6,$ip_exp[0],$ip_exp[1],$ip_exp[2],$ip_exp[3]                   // Framed-IP-Address
        );

    /* Generate Accounting Request Authenticator */
    $RA = md5($data.$radiuskey) ;

    //          v   v v     v   v   v     v     v     v     1   v
    // Line #   1   2 3     4   5   6     7     8     9     0   E
    $data=pack("CCCCH*CCCCCCCCa*CCa*CCCCCCCCCCCCCCCCCCCCCCCCCCa*CCa*CCa*CCCCCCCCCCCC",
        4,$thisidentifier,$length/256,$length%256,      // header
        $RA,                                            // authcode
        6,6,0,0,0,1,                                    // service type
        1,2+strlen($username),$username,                // username
        32,2+strlen($nasHostname[0]),$nasHostname[0],   // nasIdentifier
        5,6,0,0,0,$nas_port,                            // nasPort
        61,6,0,0,0,15,                                  // nasPortType = Ethernet
        40,6,0,0,0,1,                                   // Acct Status Type = Start
        45,6,0,0,0,1,                                   // Acct RADIUS Authenticated
        44,2+strlen($sessionid),$sessionid,             // Acct Session ID
        30,2+strlen($calledstationid),$calledstationid, // Called-Station-ID
        31,2+strlen($callingstationid),$callingstationid,                 // Calling-Station-ID
        4,6,$nas_ip_exp[0],$nas_ip_exp[1],$nas_ip_exp[2],$nas_ip_exp[3],  // nas-IP-Address
        8,6,$ip_exp[0],$ip_exp[1],$ip_exp[2],$ip_exp[3]                   // Framed-IP-Address
        );

    if($debug) {
        echo "username is $username with len " . strlen($username) ."\n" ;
        echo "nasHostname is {$nasHostname[0]} with len " . strlen($nasHostname[0]) ."\n" ;
    }

    $ret = fwrite($fd,$data) ;
    if( !$ret || ($ret != $length) ) 
        return 1; /* error return */

    if ($debug)
        echo "<br>writing $length bytes<hr>\n";

    $readdata = fgets($fd,2) ; /* read 1 byte */
    $status = socket_get_status($fd) ;
    fclose($fd) ;

    if($status['timed_out'])
        $retvalue = 1 ;
    else
        $retvalue = ord($readdata) ;

    return $retvalue ;
    // 5 -> Accounting-Response
    // See RFC2866 for this.
}

function RADIUS_ACCOUNTING_STOP($ruleno,$username,$sessionid,$start_time,$radiusip,$radiusport,$radiuskey,$clientip,$clientmac, $term_cause = 1, $interimupdate=false,$stop_time = null) {
    # $debug = 1 ;
    global $config;

    $stop_time = (empty($stop_time)) ? time() : $stop_time;
    exec("/bin/hostname", $nasHostname) ;
    if(!$nasHostname[0])
        $nasHostname[0] = "quewall" ;

    $input_pkts = $input_bytes = $input_gigawords = $output_pkts = $output_bytes = $output_gigawords = 0 ;

    exec("/sbin/ipfw show {$ruleno}", $ipfw) ;    
    preg_match("/(\d+)\s+(\d+)\s+(\d+)\s+skipto/", $ipfw[0], $matches) ;
    $input_pkts = $matches[2] ;
    $input_bytes = remainder($matches[3]) ;
    $input_gigawords = gigawords($matches[3]) ;

    unset($matches) ;
    preg_match("/(\d+)\s+(\d+)\s+(\d+)\s+skipto/", $ipfw[1], $matches) ;
    $output_pkts = $matches[2] ;
    $output_bytes = remainder($matches[3]) ;
    $output_gigawords = gigawords($matches[3]) ;

    $fd = @fsockopen("udp://$radiusip",$radiusport,$errno,$errstr,3) ;
    if(!$fd) 
        return 1 ; /* error return */

    /* set 5 second timeout on socket i/o */
    stream_set_timeout($fd, 5) ;

    $nas_ip = get_current_wan_address();
    $nas_ip_exp = explode(".",$nas_ip);
    $nas_port = $ruleno - 10000;
    $nas_mac = get_interface_mac($config['interfaces']['wan']['if']);
    $ip_exp=explode(".",$clientip);
    $session_time = $stop_time - $start_time;
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

    /* Initialise rand function, make it more random */
    srand((double)microtime() * 1000000);

    $thisidentifier=rand()%256;

    $length=4+                          // header
        16+                             // auth code
        6+                              // service type
        2+strlen($username)+            // username
        2+strlen($nasHostname[0])+      // nasIdentifier
        6+                              // nasPort
        6+                              // nasPortType
        6+                              // Acct Status Type
        6+                              // Acct RADIUS Authenticated
        2+strlen($sessionid)+           // Acct SessionID
        6+                              // Acct terminate
        6+                              // Session time
        6+                              // input bytes
        6+                              // input packets
        6+                              // input gigawords
        6+                              // output bytes
        6+                              // output packets
        6+                              // output gigawords
        2+strlen($calledstationid)+     // Called-Station-ID
        2+strlen($callingstationid)+    // Calling-Station-ID
        6+                              // nas-IP-Address
        6;                              // Framed-IP-Address

    if ($interimupdate)
        $acctstatustype = 3;
    else
        $acctstatustype = 2;

    //          v   v   v     v   v   v     v     v     v     1   1  1  1  1  1  1  1  1  v
    // Line #   1   2   3     4   5   6     7     8     9     0   1  2  3  4  5  6  7  8  E
    $data=pack("CCCCNNNNCCCCCCCCa*CCa*CCCCCCCCCCCCCCCCCCCCCCCCCCa*CCNCCNCCNCCNCCNCCNCCNCCNCCa*CCa*CCCCCCCCCCCC",
        4,$thisidentifier,$length/256,$length%256,      // header
        0,0,0,0,                                        // authcode
        6,6,0,0,0,1,                                    // service type
        1,2+strlen($username),$username,                // username
        32,2+strlen($nasHostname[0]),$nasHostname[0],   // nasIdentifier
        5,6,0,0,0,$nas_port,                            // nasPort
        61,6,0,0,0,15,                                  // nasPortType = Ethernet
        40,6,0,0,0,$acctstatustype,                     // Acct Status Type
        45,6,0,0,0,1,                                   // Acct RADIUS Authenticated
        44,2+strlen($sessionid),$sessionid,             // Acct Session ID
        49,6,$term_cause,                               // Acct Terminate
        46,6,$session_time,                             // Session Time
        42,6,$input_bytes,                              // Input Octets
        47,6,$input_pkts,                               // Input Packets
        52,6,$input_gigawords,                          // Input Gigawords
        43,6,$output_bytes,                             // Output Octets
        48,6,$output_pkts,                              // Output Packets
        53,6,$output_gigawords,                         // Output Gigawords
        30,2+strlen($calledstationid),$calledstationid, // Called-Station-ID
        31,2+strlen($callingstationid),$callingstationid,                 // Calling-Station-ID
        4,6,$nas_ip_exp[0],$nas_ip_exp[1],$nas_ip_exp[2],$nas_ip_exp[3],  // nas-IP-Address
        8,6,$ip_exp[0],$ip_exp[1],$ip_exp[2],$ip_exp[3]                   // Framed-IP-Address
        );

    /* Generate Accounting Request Authenticator */
    $RA = md5($data.$radiuskey) ;

    //          v   v v     v   v   v     v     v     v     1   1  1  1  1  1  1  1  1  v
    // Line #   1   2 3     4   5   6     7     8     9     0   1  2  3  4  5  6  7  8  E
    $data=pack("CCCCH*CCCCCCCCa*CCa*CCCCCCCCCCCCCCCCCCCCCCCCCCa*CCNCCNCCNCCNCCNCCNCCNCCNCCa*CCa*CCCCCCCCCCCC",
        4,$thisidentifier,$length/256,$length%256,      // header
        $RA,                                            // authcode
        6,6,0,0,0,1,                                    // service type
        1,2+strlen($username),$username,                // username
        32,2+strlen($nasHostname[0]),$nasHostname[0],   // nasIdentifier
        5,6,0,0,0,$nas_port,                            // nasPort
        61,6,0,0,0,15,                                  // nasPortType = Ethernet
        40,6,0,0,0,$acctstatustype,                     // Acct Status Type
        45,6,0,0,0,1,                                   // Acct RADIUS Authenticated
        44,2+strlen($sessionid),$sessionid,             // Acct Session ID
        49,6,$term_cause,                               // Acct Terminate
        46,6,$session_time,                             // Session Time
        42,6,$input_bytes,                              // Input Octets
        47,6,$input_pkts,                               // Input Packets
        52,6,$input_gigawords,                          // Input Gigawords
        43,6,$output_bytes,                             // Output Octets
        48,6,$output_pkts,                              // Output Packets
        53,6,$output_gigawords,                         // Output Gigawords
        30,2+strlen($calledstationid),$calledstationid, // Called-Station-ID
        31,2+strlen($callingstationid),$callingstationid,                 //Calling-Station-ID
        4,6,$nas_ip_exp[0],$nas_ip_exp[1],$nas_ip_exp[2],$nas_ip_exp[3],  //nas-IP-Address
        8,6,$ip_exp[0],$ip_exp[1],$ip_exp[2],$ip_exp[3]                   //Framed-IP-Address
        );

    if($debug) {
        echo "username is $username with len " . strlen($username) ."\n" ;
        echo "nasHostname is {$nasHostname[0]} with len " . strlen($nasHostname[0]) ."\n" ;
    }

    $ret = fwrite($fd,$data) ;
    if( !$ret || ($ret != $length) ) 
        return 1; /* error return */

    if ($debug)
        echo "<br>writing $length bytes<hr>\n";

    $readdata = fgets($fd,2) ; /* read 1 byte */
    $status = socket_get_status($fd) ;
    fclose($fd) ;

    if($status['timed_out'])
        $retvalue = 1 ;
    else
        $retvalue = ord($readdata) ;

    return $retvalue ;
    // 5 -> Accounting-Response
    // See RFC2866 for this.
}

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
