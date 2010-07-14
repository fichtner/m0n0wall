--- internet.c.orig	2000-09-30 19:42:51.000000000 +0200
+++ internet.c	2008-02-17 14:29:02.000000000 +0100
@@ -76,17 +76,14 @@
         alarm((unsigned int)timespan);
 
 /* Look up the Internet name or IP number. */
-
-        if (! isdigit(hostname[0])) {
-            errno = 0;
-            host = gethostbyname(hostname);
-        } else {
-            if ((ipaddr = inet_addr(hostname)) == (unsigned long)-1)
-                fatal(0,"invalid IP number %s",hostname);
+		if (inet_aton(hostname, &ipaddr)) {
             network_to_address(address,ipaddr);
             errno = 0;
             host = gethostbyaddr((void *)address,sizeof(struct in_addr),
                 AF_INET);
+		} else {
+            errno = 0;
+            host = gethostbyname(hostname);
         }
 
 /* Now clear the timer and check the result. */
