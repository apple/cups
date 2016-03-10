---
title: Common UNIX Printing System 1.1.22rc2
layout: post
permalink: /blog/:year-:month-:day-:title.html
---

<P>The second release candidate for version 1.1.22 of the CommonUNIX Printing System ("CUPS") is now available for download fromthe CUPS web site at:</P><PRE>    <A HREF="http://www.cups.org/software.html">http://www.cups.org/software.html</A></PRE><P>In accordance with the CUPS Configuration Management Plan,you now have until Wednesday, October 27th to test this releasecandidate to determine if there are any high-priority problemsand report them using the Software Trouble Report form at:</P><PRE>    <A HREF="http://www.cups.org/str.php">http://www.cups.org/str.php</A></PRE><P>Reports sent to the CUPS newsgroups or mailing lists are notautomatically entered into the trouble report database and willnot influence the final production release of 1.1.22, so it isvery important that you report any problems you identify usingthe form.</P><P>CUPS 1.1.22 is a bug fix release which fixes device URIlogging, file descriptor and memory leaks, crashes related toprinter browsing, and error handling in the browsing code. Thenew release also adds support for PostScript files from otherWindows PostScript drivers.</P><P>Changes in CUPS v1.1.22rc1:</P><UL>	
- Also sanitize device URI in argv[0] (Issue #933) 	
- cupsRasterReadHeader() didn't swap bytes for the 	numeric fields properly (Issue #930)  </UL>
