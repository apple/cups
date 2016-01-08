---
title: Common UNIX Printing System 1.2.10
layout: post
---

<P>CUPS 1.2.10 is now available for download from the CUPS web site at:</P><PRE>    <A HREF="http://www.cups.org/software.html">http://www.cups.org/software.html</A></PRE><P>CUPS 1.2.10 fixes the init script used to start the scheduler, a recursion bug in the pdftops filter, and several other issues reported after the 1.2.9 release. Changes include:</P>
- ppdLocalize() now supports localizing for Japanese using the "jp" locale name used by the ppdmerge program from the CUPS DDK 1.1.0 (Issue #2301) 
- _cupsAdminSetServerSettings() did not support changing of top-level directives as designed. 
- The init script path check was broken. 
- CUPS incorrectly used the attribute "notify-recipient" instead of "notify-recicpient-uri" in several places (Issue #2297) 
- Fixed a configure script bug on MirBSD (Issue #2294) 
- The pdftops filter did not limit the amount of recursion of page sets (Issue #2293) 
- Custom page sizes with fractional point sizes did not work (Issue #2296) 
- The lpoptions command would crash when adding or removing options on a system with no printers (Issue #2295)
