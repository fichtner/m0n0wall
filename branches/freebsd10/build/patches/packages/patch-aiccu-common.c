--- common/common.c.orig	2006-12-21 15:08:50.000000000 +0100
+++ common/common.c	2012-01-19 10:13:03.000000000 +0100
@@ -37,7 +37,7 @@
 	if (g_aiccu && !g_aiccu->verbose && level == LOG_DEBUG) return;
 
 #ifndef _WIN32
-	if (g_aiccu && g_aiccu->daemonize > 0) vsyslog(LOG_LOCAL7|level, fmt, ap);
+	if (g_aiccu && g_aiccu->daemonize > 0) vsyslog(LOG_DAEMON|level, fmt, ap);
 	else
 	{
 		vfprintf(stderr, fmt, ap);
