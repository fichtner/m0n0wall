--- isakmp_quick.c.orig	Tue Jan 11 02:09:50 2005
+++ isakmp_quick.c	Wed Sep  7 17:45:47 2005
@@ -2031,6 +2031,21 @@
 			"no policy found: %s\n", spidx2str(&spidx));
 		return ISAKMP_INTERNAL_ERROR;
 	}
+	
+	/* Refresh existing generated policies
+	*/
+	if (iph2->ph1->rmconf->gen_policy) {
+		   plog(LLV_INFO, LOCATION, NULL,
+					"Update the generated policy : %s\n",
+					spidx2str(&spidx));
+		   iph2->spidx_gen = racoon_malloc(sizeof(spidx));
+		   if (!iph2->spidx_gen) {
+				   plog(LLV_ERROR, LOCATION, NULL,
+							"buffer allocation failed.\n");
+				   return ISAKMP_INTERNAL_ERROR;
+		   }
+		   memcpy(iph2->spidx_gen, &spidx, sizeof(spidx));
+	}
 
 	/* get outbound policy */
     {
