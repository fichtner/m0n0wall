
function wlan_enable_change(enable_over) {

	var if_enable = (document.iform.enable.checked || enable_over);
	
	// WPA only in hostap mode
	var wpa_enable = (if_enable && document.iform.mode.options[document.iform.mode.selectedIndex].value == "hostap") || enable_over;
	var wpa_opt_enable = (if_enable && document.iform.wpamode.options[document.iform.wpamode.selectedIndex].value != "none") || enable_over;
	
	// enter WPA PSK only in PSK mode
	var wpa_psk_enable = (if_enable && document.iform.wpamode.options[document.iform.wpamode.selectedIndex].value == "psk") || enable_over;
	
	// RADIUS server only in WPA Enterprise mode
	var wpa_ent_enable = (if_enable && document.iform.wpamode.options[document.iform.wpamode.selectedIndex].value == "enterprise") || enable_over;
	
	// WEP only if WPA is disabled
	var wep_enable = (if_enable && document.iform.wpamode.options[document.iform.wpamode.selectedIndex].value == "none") || enable_over;
	
	document.iform.standard.disabled = !if_enable;
	document.iform.mode.disabled = !if_enable;
	document.iform.ssid.disabled = !if_enable;
	document.iform.channel.disabled = !if_enable;
	document.iform.wep_enable.disabled = !wep_enable;
	document.iform.key1.disabled = !wep_enable;
	document.iform.key2.disabled = !wep_enable;
	document.iform.key3.disabled = !wep_enable;
	document.iform.key4.disabled = !wep_enable;
	
	document.iform.wpamode.disabled = !if_enable;
	document.iform.wpaversion.disabled = !wpa_opt_enable;
	document.iform.wpacipher.disabled = !wpa_opt_enable;
	document.iform.wpapsk.disabled = !wpa_psk_enable;
	document.iform.radiusip.disabled = !wpa_ent_enable;
	document.iform.radiusauthport.disabled = !wpa_ent_enable;
	document.iform.radiusacctport.disabled = !wpa_ent_enable;
	document.iform.radiussecret.disabled = !wpa_ent_enable;
}
