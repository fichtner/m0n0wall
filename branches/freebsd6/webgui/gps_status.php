#!/usr/local/bin/php
<?php
$pgtitle = array("Diagnostics", "GPS Information");


require("guiconfig.inc");
include("fbegin.inc"); 

ob_start();
?>
<style media="screen" type="text/css">
.redhb {
        padding: 1px;
        border: 1px solid #808080;
        background-color: #D00000;
        color: #C0C0C0;
        height: 10px;
        font-size: x-small;
}
.greenhb {
        border: 1px solid #808080;
        background-color: #33CC33;
        color: #000000;
        height: 10px;
        font-size: x-small;
}
table.satsig TD {
        border: 1px solid #808080;
        padding: 4px;
        color: #999;
        vertical-align: bottom;
}

.satr {
        color: #808080;
        border-style: none solid none none;
        border-width: 1px;
        border-color: #808080;
}
.satsigt {
        border: 1px solid #808080;
}
.satrt {
        border: 1px solid #808080;
        color: #808080;
        text-align: center;
        font-size: small;
}
.sattitle {
        color: #808080;
        border-style: none;
        border-color: #808080;
}


</style>

<body>
<table width="100%" border="0" cellpadding="0" cellspacing="0" summary="tab pane">
  <tr><td class="tabnavtbl">
  <ul id="tabnav">
<?php
   	$tabs = array('NTP Server' => 'ntp_status.php',
           		  'GPS Information' => 'gps_status.php');
	dynamic_tab_menu($tabs);
?> 
 </ul>
  </td></tr>
  <tr> 
    <td class="tabcont">
	<table id="gpsdata" width="100%" border="0" cellspacing="0" cellpadding="0" summary="content pane"> 
	            <tr><td colspan="2" class="listtopic">GGA Data</td></tr> 
	            <tr> 
	                <td width="22%" class="vncellt">Time of Fix</td> 
	                <td width="78%" class="listr">Reading...</td> 
	            </tr>
				<tr>
					<td width="22%" class="vncellt">Latitude</td>
					<td width="78%" class="listr">...</td> 
				</tr>
				<tr>
					<td width="22%" class="vncellt">Longitude</td>
					<td width="78%" class="listr">...</td> 
				</tr>
				<tr>
					<td width="22%" class="vncellt">Fix Quality</td>
					<td width="78%" class="listr">...</td> 
				</tr>
				<tr>
					<td width="22%" class="vncellt"># Satellites Tracked</td>
					<td width="78%" class="listr">...</td> 
				</tr>
				<tr>
					<td width="22%" class="vncellt">HDOP</td>
					<td width="78%" class="listr">...</td> 
				</tr>
				<tr>
					<td width="22%" class="vncellt">Altitude</td>
					<td width="78%" class="listr">...</td> 
				</tr>
				<tr>
					<td width="22%" class="vncellt">Height of geoid</td>
					<td width="78%" class="listr">...</td> 
				</tr>
				<tr>
					<td width="22%" class="vncellt">Last DGPS update</td>
					<td width="78%" class="listr">...</td> 
				</tr>
				<tr>
					<td width="22%" class="vncellt">DGPS Station ID</td>
					<td width="78%" class="listr">...</td> 
				</tr>
				<tr><td colspan="8" class="list" height="12"></td></tr>

				<tr><td colspan="2" class="listtopic">RMC Data</td></tr> 
	              <tr> 
	                <td width="22%" class="vncellt">Time of Fix</td> 
	                <td width="78%" class="listr">Reading...</td> 
	              </tr>

				<tr>
					<td width="22%" class="vncellt">Status</td>
					<td width="78%" class="listr">...</td> 
				</tr>
				<tr>
					<td width="22%" class="vncellt">Latitude</td>
					<td width="78%" class="listr">...</td> 
				</tr>
				<tr>
					<td width="22%" class="vncellt">Longitude</td>
					<td width="78%" class="listr">...</td> 
				</tr>
				<tr>
					<td width="22%" class="vncellt">Speed over ground</td>
					<td width="78%" class="listr">...</td> 
				</tr>
				<tr>
					<td width="22%" class="vncellt">Track Angle</td>
					<td width="78%" class="listr">...</td> 
				</tr>
				<tr>
					<td width="22%" class="vncellt">Date</td>
					<td width="78%" class="listr">...</td> 
				</tr>
				<tr>
					<td width="22%" class="vncellt">Magnetic Variation</td>
					<td width="78%" class="listr">...</td> 
				</tr>

				<tr><td colspan="8" class="list" height="12"></td></tr>

				<tr><td colspan="2" class="listtopic">GSA Data</td></tr> 
	    <tr> 
	      <td width="22%" class="vncellt">Fix Type</td> 
	      <td width="78%" class="listr">Reading...
		  <?php
				if($gpgsa[1]=="A") $gpgsa[1]="Auto";
				if($gpgsa[1]=="M") $gpgsa[1]="Manual";
				echo $gpgsa[1];
			?></td> 
	    </tr>

		<tr>
			<td width="22%" class="vncellt">Fix Status</td>
			<td width="78%" class="listr">...</td> 
		</tr>
		<tr>
			<td width="22%" class="vncellt">Satellites used</td>
			<td width="78%" class="listr">...</td> 
		</tr>
		<tr>
			<td width="22%" class="vncellt">PDOP</td>
			<td width="78%" class="listr">...</td> 
		</tr>
		<tr>
			<td width="22%" class="vncellt">HDOP</td>
			<td width="78%" class="listr">...</td> 
		</tr>
		<tr>
			<td width="22%" class="vncellt">VDOP</td>
			<td width="78%" class="listr">...</td> 
		</tr>
		<tr><td colspan="8" class="list" height="12"></td></tr>
		<tr><td colspan="2" class="listtopic">Signal Data</td></tr>
		<tr>
			<td width="22%" class="vncellt">Satellite signal details</td>
			<td width="78%" class="listr"><div id="satellite_info"></div></td>
	</tr>
	</table>	
	</td>
  </tr>
</table>		
<script type='text/javascript' src='gps_status_vals.php' defer='defer' ></script>
</body>
<?php
echo(str_repeat(' ',4096));
ob_flush();
flush();
?>