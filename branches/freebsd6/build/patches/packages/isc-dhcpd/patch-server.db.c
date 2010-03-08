--- server/db.c.orig	2008-01-22 18:28:24.000000000 +0100
+++ server/db.c	2010-02-24 22:46:06.000000000 +0100
@@ -718,7 +718,7 @@
 	/* If we haven't rewritten the lease database in over an
 	   hour, rewrite it now.  (The length of time should probably
 	   be configurable. */
-	if (count && cur_time - write_time > 3600) {
+	if (count && cur_time - write_time > 300) {
 		count = 0;
 		write_time = cur_time;
 		new_lease_file ();
