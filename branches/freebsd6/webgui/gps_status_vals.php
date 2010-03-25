#!/usr/local/bin/php
<?php

$fp = @fopen("/dev/gps0","r");

$line=str_repeat('Unknown,',20);
$gpgga=split(",",$line);
$gprmc=split(",",$line);
$gpgsa=split(",",$line);

if ($fp){
	while (($line = fgets($fp)) && ((!$gotgpgsv) || (!$gotgpgga) || (!$gotgprmc) || (!$gotgpgsa))) {
	if (strpos($line, "*") !== false) {
		switch (substr($line,1,5)) {
		case GPGSV:
			$i=1;
			$gdata = $line;
			$gpgsv[0]=split("[*,]",$line);
			if ($gpgsv[0][2] == 1 ){
				$s=1;
			    while ($s <= 4 ) {
					$satinfo[$s][1] = $gpgsv[0][(4*$s)];
					$satinfo[$s][2] = $gpgsv[0][3+(4*$s)];
                    $s++;
				}
                while ($i <= $gpgsv[0][1]-1) {
            	    $line=fgets($fp);
	                if (substr($line,1,5) == "GPGSV") {
	                    $gpgsv[($i)]=split("[*,]",$line);
	                    $s=1;
	                     while ($s <= 4 ) {
	                     $satinfo[(($i)*4)+$s][1] = $gpgsv[$i][(4*$s)];
	                     $satinfo[(($i)*4)+$s][2] = $gpgsv[$i][3+(4*$s)];
	                     $s++;
	                    }
	                $i++;
	                }
	           	}
				if ($i >=$gpgsv[0][1]-1) {
				$gotgpgsv = true;
				}
             	break;
			}
			break;
		case GPGGA:
		        $gpgga=split("[*,]",$line);
				$gotgpgga = true;
				break;
		case GPRMC:
		        $gprmc=split("[*,]",$line);
				$gotgprmc = true;
				break;
		case GPGSA:
		        $gpgsa=split("[*,]",$line);
				$gotgpgsa = true;
				break;
		}
		}
	}
}

fclose($fp);
$k=1;
$sattable ="<table id='satinfo' cellpadding='0' cellspacing='1' class='sattitle'><tr><td class='satrt'>Sat ID</td><td style='width: 94px'><div  style='width: 102px' class='satrt'>Signal</div></td></tr>";
while ($k <= $gpgsv[0][3]) {
	$p = $satinfo[$k][2]*3;
	$sattable .= "  <tr><td class='satrt'>{$satinfo[$k][1]}</td><td style='width: {$satinfo[$k][2]}'><div class='greenhb' style='width: {$p}px'> {$satinfo[$k][2]}</div></td></tr>";
	$k++;
}
$sattable .= "</table>";
?>

document.getElementById('gpsdata').rows[1].cells[1].innerHTML="<?=substr_replace($gpgga[1],rtrim(chunk_split(substr($gpgga[1],0,6),2,":"),":"),0,6);?> UTC";
document.getElementById('gpsdata').rows[2].cells[1].innerHTML="<a href='http://maps.google.com/maps?q=<?=substr_replace($gpgga[2],'&deg;',2,0);?><?=$gpgga[3];?>,<?=substr_replace($gpgga[4],'&deg;',3,0);?><?=$gpgga[5];?>' target='_blank'><?=substr_replace($gpgga[2],'&deg;',2,0);?> <?=$gpgga[3];?></a>";
document.getElementById('gpsdata').rows[3].cells[1].innerHTML="<?=substr_replace($gpgga[4],'&deg;',3,0);?> <?=$gpgga[5];?>";
document.getElementById('gpsdata').rows[4].cells[1].innerHTML="<?php
														if($gpgga[6]=="0") $gpgga[6]="Invalid";
                                                        if($gpgga[6]=="1") $gpgga[6]="GPS Fix";
                                                        if($gpgga[6]=="2") $gpgga[6]="Differential GPS Fix";
                                                        echo $gpgga[6];
														?>";
document.getElementById('gpsdata').rows[5].cells[1].innerHTML="<?=$gpgga[7];?>";
document.getElementById('gpsdata').rows[6].cells[1].innerHTML="<?=$gpgga[8];?>";
document.getElementById('gpsdata').rows[7].cells[1].innerHTML="<?=$gpgga[9];?> <?=$gpgga[10];?>";
document.getElementById('gpsdata').rows[8].cells[1].innerHTML="<?=$gpgga[11];?> <?=$gpgga[12];?>";
document.getElementById('gpsdata').rows[9].cells[1].innerHTML="<?=$gpgga[13];?> Seconds ago";
document.getElementById('gpsdata').rows[10].cells[1].innerHTML="<?=$gpgga[14];?>";

document.getElementById('gpsdata').rows[13].cells[1].innerHTML="<?=substr_replace($gprmc[1],rtrim(chunk_split(substr($gprmc[1],0,6),2,":"),":"),0,6);?> UTC";
document.getElementById('gpsdata').rows[14].cells[1].innerHTML="<?php
														if($gprmc[2]=="A") $gprmc[2]="Data Valid";
                                                        if($gprmc[2]=="V") $gprmc[2]="Data Invalid";
                                                        echo $gprmc[2];
														?>";
document.getElementById('gpsdata').rows[15].cells[1].innerHTML="<?=substr_replace($gprmc[3],'&deg;',2,0);?> <?=$gprmc[4];?> ";
document.getElementById('gpsdata').rows[16].cells[1].innerHTML="<?=substr_replace($gprmc[5],'&deg;',3,0);?> <?=$gprmc[6];?> ";
document.getElementById('gpsdata').rows[17].cells[1].innerHTML="<?=$gprmc[7];?> Knots ";
document.getElementById('gpsdata').rows[18].cells[1].innerHTML="<?=$gprmc[8];?> Degrees true ";
document.getElementById('gpsdata').rows[19].cells[1].innerHTML="<?=rtrim(chunk_split($gprmc[9],2,"/"),"/");?> ";
document.getElementById('gpsdata').rows[20].cells[1].innerHTML="<?=$gprmc[10];?> <?=$gprmc[11];?>";

document.getElementById('gpsdata').rows[23].cells[1].innerHTML="<?php
															if($gpgsa[1]=="A") $gpgsa[1]="Auto";
															if($gpgsa[1]=="M") $gpgsa[1]="Manual";
															echo $gpgsa[1];
															?>";
document.getElementById('gpsdata').rows[24].cells[1].innerHTML="<?php
															if($gpgsa[2]=="1") $gpgsa[2]="Fix Not Available";
															if($gpgsa[2]=="2") $gpgsa[2]="2D";
															if($gpgsa[2]=="3") $gpgsa[2]="3D";
															echo $gpgsa[2];
															?>";
document.getElementById('gpsdata').rows[25].cells[1].innerHTML="<?=$gpgsa[3];?>,<?=rtrim(implode(array_slice($gpgsa,4,11),","),",");?> ";
document.getElementById('gpsdata').rows[26].cells[1].innerHTML="<?=$gpgsa[15];?>";
document.getElementById('gpsdata').rows[27].cells[1].innerHTML="<?=$gpgsa[16];?>";
document.getElementById('gpsdata').rows[28].cells[1].innerHTML="<?=$gpgsa[17];?>";


document.getElementById('satellite_info').innerHTML="<?=$sattable;?>";