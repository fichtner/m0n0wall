<?php
/* vim: set expandtab tabstop=4 shiftwidth=4: */
/*
$Id$

Copyright (c) 2005, Jonathan De Graeve <jonathan.de.graeve@imelda.be>
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
    * Support for multiple radius servers
    * Error Message and Reply Message
    * Called-Station-Id,Calling-Station-Id,NAS-Port,NAS-Port-Type
    * Different Authentication Methods

*/

require_once("radius.inc");

/*
RADIUS AUTHENTICATION
---------------------
*/

function RADIUS_AUTHENTICATION($username,$password,$radiusservers,$clientip,$clientmac,$ruleno) {
    global $config;

    /* Initialisation of variables - Constructor */
    $retvalue = array();
    $retvalue['error'] = $retvalue['reply_message'] = $retvalue['url_redirection'] = $retvalue['session_timeout'] = $retvalue['idle_timeout'] = $retvalue['session_terminate_time'] = null;
    $nas_mac = get_interface_mac($config['interfaces']['wan']['if']);
    $nas_port = $ruleno - 10000;
    $radiusvendor = $config['captiveportal']['radiusvendor'] ? $config['captiveportal']['radiusvendor'] : null;

    exec("/bin/hostname", $nasHostname) ;
    if(!$nasHostname[0])
    $nasHostname[0] = "m0n0wall" ;
    
$rauth = new Auth_RADIUS_PAP($username, $password);

/*
Add support for more then one radiusserver. 
At most 10 servers may be specified. 
When multiple servers are given, they are tried in round-robin fashion until a valid response is received 
*/

foreach ($radiusservers as $radsrv) {

    // Add a new server to our instance
    $rauth->addServer($radsrv['ipaddr'], $radsrv['port'], $radsrv['key']);
    
}

$rauth->username = $username;
$rauth->password = $password;


if (!$rauth->start()) {
    $retvalue['auth_val'] = 1;
    $retvalue['error'] = $rauth->getError(); 
    if ($debug)
        printf("Radius start: %s<br>\n", $retvalue['error']);
}
else {

    // 1 -> Access-Request => We will use this value as an error indicator since we can't get a 1 back from the radius
    // 2 -> Access-Accept
    // 3 -> Access-Reject
    // See RFC2865 for this.

    /*
     * We put our attributes in here
     */

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
    $rauth->putAttribute(RADIUS_NAS_PORT, $nas_port);

    // Send request

    $result = $rauth->send();
    if (PEAR::isError($result)) {
        $retvalue['auth_val'] = 1;
        $retvalue['error'] = $result->getMessage();
        if ($debug)
            printf("Radius send failed: %s<br>\n", $retvalue['error']);
    } else if ($result === true) {
        $retvalue['auth_val'] = 2;
        if ($debug)
            printf("Radius Auth succeeded<br>\n");
    } else {
        $retvalue['auth_val'] = 3;
        if ($debug)
            printf("Radius Auth rejected<br>\n");
    }

    // Get attributes, even if auth failed.
    // We will push the results in the retvalue array
    if (!$rauth->getAttributes()) {
        $retvalue['error'] = $rauth->getError();
        if ($debug)
            printf("Radius getAttributes: No attributes<br>\n", $retvalue['error']);
    } else {
        $retvalue = array_merge($retvalue,$rauth->listAttributes());
        if ($debug) {
            if (!$rauth->listAttributes())
                printf("No Attributes<br>\n");
            else
            print_r($rauth->listAttributes());
        }
        // We convert the session_terminate_time to unixtimestamp if its set before returning the whole array to our caller
        if (!empty($retvalue['session_terminate_time'])) {
        $stt = &$retvalue['session_terminate_time'];
        $stt = strtotime(preg_replace("/\+(\d+):(\d+)$/", " +\${1}\${2}", preg_replace("/(\d+)T(\d+)/", "\${1} \${2}",$stt)));
         }
    }
     }

     // close OO RADIUS_AUTHENTICATION
     $rauth->close();

     return $retvalue;

}

?>
