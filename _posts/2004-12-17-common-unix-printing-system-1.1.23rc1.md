---
title: Common UNIX Printing System 1.1.23rc1
layout: post
permalink: /blog/:year-:month-:day-:title.html
---

<P>The first release candidate for version 1.1.23 of the CommonUNIX Printing System ("CUPS") is now available for download fromthe CUPS web site at:</P><PRE>    <A HREF="http://www.cups.org/software.html">http://www.cups.org/software.html</A></PRE><P>In accordance with the CUPS Configuration Management Plan,you now have until Friday, December 31st to test this releasecandidate to determine if there are any high-priority problemsand report them using the Software Trouble Report form at:</P><PRE>    <A HREF="http://www.cups.org/str.php">http://www.cups.org/str.php</A></PRE><P>Reports sent to the CUPS newsgroups or mailing lists are notautomatically entered into the trouble report database and willnot influence the final production release of 1.1.23, so it isvery important that you report any problems you identify usingthe form.</P><P>CUPS 1.1.23 is a bug fix release which fixes two securityvulnerabilities reported by Daniel J. Bernstein (djb@cr.yp.to).The new release also contains other minor bug and documentationfixes that are not security related.</P><P>Changes in CUPS v1.1.23rc1:</P><UL>	
- The lpr man page did not document the "-U" option (Issue #998) 	
- The scheduler no longer sends the page-set option when 	printing banner pages (Issue #995) 	
- Fixed a debug message in the imagetops filter (Issue #1012) 	
- The lprm man page listed the "-" option in the wrong 	order (Issue #911) 	
- The hpgltops filter contained two buffer overflows 	that could potentially allow remote access to the "lp" 	account (Issue #1024) 	
- The lppasswd command did not protect against file 	descriptor or ulimit attacks (Issue #1023) 	
- The "lpc status" command used the wrong resource path 	when querying the list of printers and jobs, causing 	unnecessary authentication requests (Issue #1018) 	
- The httpWait() function did not handle signal 	interruptions (Issue #1020) 	
- The USB backend used the wrong size status variable 	when checking the printer status (Issue #1017) 	
- The scheduler did not delete classes from other 	classes or implicit classes, which could cause a crash 	(Issue #1015) 	
- The IPP backend now logs the remote print job ID at 	log level NOTICE instead of INFO (so it shows up in 	the error_log file...)  </UL>
