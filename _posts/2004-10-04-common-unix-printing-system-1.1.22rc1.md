---
title: Common UNIX Printing System 1.1.22rc1
layout: post
---

<P>The first release candidate for version 1.1.22 of the CommonUNIX Printing System ("CUPS") is now available for download fromthe CUPS web site at:</P><PRE>    <A HREF="http://www.cups.org/software.html">http://www.cups.org/software.html</A></PRE><P>In accordance with the CUPS Configuration Management Plan,you now have until Monday, Ocotber 18th to test this releasecandidate to determine if there are any high-priority problemsand report them using the Software Trouble Report form at:</P><PRE>    <A HREF="http://www.cups.org/str.php">http://www.cups.org/str.php</A></PRE><P>Reports sent to the CUPS newsgroups or mailing lists are notautomatically entered into the trouble report database and willnot influence the final production release of 1.1.22, so it isvery important that you report any problems you identify usingthe form.</P><P>CUPS 1.1.22 is a bug fix release which fixes device URIlogging, file descriptor and memory leaks, crashes related toprinter browsing, and error handling in the browsing code. Thenew release also adds support for PostScript files from otherWindows PostScript drivers.</P><P>Changes in CUPS v1.1.22rc1:</P><UL>	
- Now sanitize the device URI that is reported in the 	error_log file (Issue #920) 	
- Fixed some memory and file descriptor leaks in the job 	dispatch code (Issue #921) 	
- Deleting a printer could cause a crash with browsing 	enabled (Issue #865, Issue #881, Issue #928) 	
- Browsing would turn off if the scheduler got an EAGAIN 	error (Issue #924) 	
- The mime.types file didn't recognize PostScript as a 	PJL language name (Issue #925)  </UL>
