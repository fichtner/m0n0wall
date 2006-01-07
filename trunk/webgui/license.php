#!/usr/local/bin/php
<?php require("guiconfig.inc"); 
?>
<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01 Transitional//EN">
<html>
<head>
<title>m0n0wall webGUI - License</title>
<meta http-equiv="Content-Type" content="text/html; charset=iso-8859-1">
<link href="gui.css" rel="stylesheet" type="text/css">
</head>

<body link="#0000CC" vlink="#0000CC" alink="#0000CC">
<?php include("fbegin.inc"); ?>
<p class="pgtitle">License</p>
            <p><strong>m0n0wall is Copyright &copy; 2002-2004 by Manuel Kasper 
              (<a href="mailto:mk@neon1.net">mk@neon1.net</a>).<br>
              All rights reserved.</strong></p>
            <p> Redistribution and use in source and binary forms, with or without<br>
              modification, are permitted provided that the following conditions 
              are met:<br>
              <br>
              1. Redistributions of source code must retain the above copyright 
              notice,<br>
              this list of conditions and the following disclaimer.<br>
              <br>
              2. Redistributions in binary form must reproduce the above copyright<br>
              notice, this list of conditions and the following disclaimer in 
              the<br>
              documentation and/or other materials provided with the distribution.<br>
              <br>
              <strong>THIS SOFTWARE IS PROVIDED &quot;AS IS'' AND ANY EXPRESS 
              OR IMPLIED WARRANTIES,<br>
              INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY<br>
              AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT 
              SHALL THE<br>
              AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, 
              EXEMPLARY,<br>
              OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT 
              OF<br>
              SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR 
              BUSINESS<br>
              INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER 
              IN<br>
              CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)<br>
              ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED 
              OF THE<br>
              POSSIBILITY OF SUCH DAMAGE</strong>.</p>
            <hr size="1">
            <p>The following persons have contributed code to m0n0wall:</p>
            <p>Bob Zoller (<a href="mailto:bob@kludgebox.com">bob@kludgebox.com</a>)<br>
              &nbsp;&nbsp;&nbsp;&nbsp;<em><font color="#666666">Diagnostics: Ping 
              function; WLAN channel auto-select; DNS forwarder</font></em><br>
              <br>
              Michael Mee (<a href="mailto:mikemee2002@pobox.com">mikemee2002@pobox.com</a>)<br>
              &nbsp;&nbsp;&nbsp;&nbsp;<em><font color="#666666">Timezone and NTP 
              client support</font></em><br>
              <br>
              Magne Andreassen (<a href="mailto:magne.andreassen@bluezone.no">magne.andreassen@bluezone.no</a>)<br>
              &nbsp;&nbsp;&nbsp;&nbsp;<em><font color="#666666">Remote syslog'ing; 
              some code bits for DHCP server on optional interfaces</font></em><br>
              <br>
              Rob Whyte (<a href="mailto:rob@g-labs.com">rob@g-labs.com</a>)<br>
              &nbsp;&nbsp;&nbsp;&nbsp;<em><font color="#666666">Idea/code bits 
              for encrypted webGUI passwords; minimalized SNMP agent</font></em><br>
              <br>
              Petr Verner (<a href="mailto:verner@ipps.cz">verner@ipps.cz</a>)<br>
              &nbsp;&nbsp;&nbsp;&nbsp;<em><font color="#666666">Advanced outbound 
              NAT: destination selection</font></em><br>
              <br>
              Bruce A. Mah (<a href="mailto:bmah@acm.org">bmah@acm.org</a>)<br>
              &nbsp;&nbsp;&nbsp;&nbsp;<em><font color="#666666">Filtering bridge 
              patches </font></em><br>
              <br>
              Jim McBeath (<a href="mailto:monowall@j.jimmc.org">monowall@j.jimmc.org</a>)<br>
              &nbsp;&nbsp;&nbsp;&nbsp;<em><font color="#666666">Filter rule patches 
              (ordering, block/pass, disabled); better status page;<br>
              &nbsp;&nbsp;&nbsp;&nbsp;webGUI assign network ports page </font></em><br>
              <br>
              Chris Olive (<a href="mailto:chris@technologEase.com">chris@technologEase.com</a>)<br>
              &nbsp;&nbsp;&nbsp;&nbsp;<em><font color="#666666">enhanced &quot;execute 
              command&quot; page</font></em><br>
              <br>
              Pauline Middelink (<a href="mailto:middelink@polyware.nl">middelink@polyware.nl</a>)<br>
              &nbsp;&nbsp;&nbsp;&nbsp;<em><font color="#666666">DHCP client: send hostname patch</font></em><br>
              <br>
              Björn Pålsson (<a href="mailto:bjorn@networksab.com">bjorn@networksab.com</a>)<br>
              &nbsp;&nbsp;&nbsp;&nbsp;<em><font color="#666666">DHCP lease list page</font></em><br>
              <br>
              Peter Allgeyer (<a href="mailto:allgeyer@web.de">allgeyer@web.de</a>)<br>
              &nbsp;&nbsp;&nbsp;&nbsp;<em><font color="#666666">&quot;reject&quot; type filter rules</font></em></p>
            <hr size="1">
            <p>m0n0wall is based upon/includes various free software packages, 
              listed below.<br>
              The author of m0n0wall would like to thank the authors of these 
              software packages for their efforts.</p>
            <p>FreeBSD (<a href="http://www.freebsd.org" target="_blank">http://www.freebsd.org</a>)<br>
              Copyright &copy; 1994-2003 FreeBSD, Inc. All rights reserved.<br>
              <br>
              This product includes PHP, freely available from <a href="http://www.php.net/" target="_blank">http://www.php.net</a>.<br>
              Copyright &copy; 1999 - 2003 The PHP Group. All rights reserved.<br>
              <br>
              mini_httpd (<a href="http://www.acme.com/software/mini_httpd" target="_blank">http://www.acme.com/software/mini_httpd)</a><br>
              Copyright &copy; 1999, 2000 by Jef Poskanzer &lt;jef@acme.com&gt;. 
              All rights reserved.<br>
              <br>
              ISC DHCP server (<a href="http://www.isc.org/products/DHCP/" target="_blank">http://www.isc.org/products/DHCP</a>)<br>
              Copyright &copy; 1996-2003 Internet Software Consortium. All rights 
              reserved.<br>
              <br>
              ipfilter (<a href="http://www.ipfilter.org" target="_blank">http://www.ipfilter.org</a>)<br>
              Copyright &copy; 1993-2002 by Darren Reed.<br>
              <br>
              MPD - Multi-link PPP daemon for FreeBSD (<a href="http://www.dellroad.org/mpd" target="_blank">http://www.dellroad.org/mpd</a>)<br>
              Copyright &copy; 1995-1999 Whistle Communications, Inc. All rights 
              reserved. <br>
              <br>
              ez-ipupdate (<a href="http://www.gusnet.cx/proj/ez-ipupdate/" target="_blank">http://www.gusnet.cx/proj/ez-ipupdate</a>)<br>
              Copyright &copy; 1998-2001 Angus Mackay. All rights reserved.<br>
              <br>
              Circular log support for FreeBSD syslogd (<a href="http://software.wwwi.com/syslogd/" target="_blank">http://software.wwwi.com/syslogd</a>)<br>
              Copyright &copy; 2001 Jeff Wheelhouse (jdw@wwwi.com)<br>
              <br>
              Dnsmasq - a DNS forwarder for NAT firewalls (<a href="http://www.thekelleys.org.uk" target="_blank">http://www.thekelleys.org.uk</a>)<br>
              Copyright &copy; 2000-2003 Simon Kelley.<br>
              <br>
              Racoon (<a href="http://www.kame.net/racoon" target="_blank">http://www.kame.net/racoon</a>)<br>
              Copyright &copy; 1995-2002 WIDE Project. All rights reserved.<br>
              <br>
              msntp (<a href="http://www.hpcf.cam.ac.uk/export" target="_blank">http://www.hpcf.cam.ac.uk/export</a>)<br>
              Copyright &copy; 1996, 1997, 2000 N.M. Maclaren, University of Cambridge. 
              All rights reserved.<br>
              <br>
              UCD-SNMP (<a href="http://www.ece.ucdavis.edu/ucd-snmp" target="_blank">http://www.ece.ucdavis.edu/ucd-snmp</a>)<br>
              Copyright &copy; 1989, 1991, 1992 by Carnegie Mellon University.<br>
              Copyright &copy; 1996, 1998-2000 The Regents of the University of 
              California. All rights reserved.<br>
              Copyright &copy; 2001-2002, Network Associates Technology, Inc. 
              All rights reserved.<br>
              Portions of this code are copyright &copy; 2001-2002, Cambridge 
              Broadband Ltd. All rights reserved.<br>
              <br>
              choparp (<a href="http://choparp.sourceforge.net/" target="_blank">http://choparp.sourceforge.net</a>)<br>
              Copyright &copy; 1997 Takamichi Tateoka (tree@mma.club.uec.ac.jp)<br>
			  Copyright
&copy; 2002 Thomas Quinot (thomas@cuivre.fr.eu.org)
<?php include("fend.inc"); ?>
</body>
</html>
