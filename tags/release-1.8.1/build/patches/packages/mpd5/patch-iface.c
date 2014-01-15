--- src/iface.c.orig	2011-12-21 15:58:49.000000000 +0100
+++ src/iface.c	2012-02-16 15:24:51.000000000 +0100
@@ -1902,8 +1902,14 @@
 
 	res = ioctl(s, add?SIOCAIFADDR_IN6:SIOCDIFADDR_IN6, &ifra6);
 	if (res == -1) {
-	    Perror("[%s] IFACE: %s IPv6 address %s %s failed", 
-		b->name, add?"Adding":"Removing", add?"to":"from", b->iface.ifname);
+		if (add && errno == EEXIST) {
+			/* this can happen if the kernel has already automatically added
+			   the same link-local address - ignore the error */
+			res = 0;
+		} else {
+			Perror("[%s] IFACE: %s IPv6 address %s %s failed", 
+				b->name, add?"Adding":"Removing", add?"to":"from", b->iface.ifname);
+		}
 	}
 	break;
 
