--- server/db.c.orig	2012-08-23 20:23:54.000000000 +0200
+++ server/db.c	2012-10-17 16:24:04.000000000 +0200
@@ -36,7 +36,7 @@
 #include <ctype.h>
 #include <errno.h>
 
-#define LEASE_REWRITE_PERIOD 3600
+#define LEASE_REWRITE_PERIOD 300
 
 static isc_result_t write_binding_scope(FILE *db_file, struct binding *bnd,
 					char *prepend);
