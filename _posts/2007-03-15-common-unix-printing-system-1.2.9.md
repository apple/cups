---
title: Common UNIX Printing System 1.2.9
layout: post
---

<P>CUPS 1.2.9 is now available for download from the CUPS web site at:</P><PRE>    <A HREF="http://www.cups.org/software.html">http://www.cups.org/software.html</A></PRE><P>CUPS 1.2.9 fixes several printing issues and scheduler crash bug. Changes include:</P>
- The scheduler did not use the default job-sheets (banners) for implicit classes (Issue #2284) 
- The scheduler could crash when listing complete jobs that had been unloaded from memory (Issue #2288) 
- The French localization was doubled up (Issue #2287) 
- Build system fixes for several platforms (Issue #2260, Issue #2275) 
- The scheduler's openssl certificate generation code was broken on some platforms (Issue #2282) 
- The scheduler's log rotation check for devices was broken (Issue #2278) 
- The LPD mini-daemon did not handle the document-format option correctly (Issue #2266) 
- The pdftops filter ignored the "match" size option in the pdftops.conf file (Issue #2285) 
- cupstestppd now validates UTF-8 text strings in globalized PPD files (Issue #2283) 
- The outputorder=reverse option did not work with all printers (Issue #2279) 
- Classes containing other classes did not always work (Issue #2255) 
- Printer location and description information was lost if the corresponding string contained the "#" character (Issue #2254) 
- cupsRemoveOption() did not work properly (Issue #2264) 
- The USB backend did not work with some USB to parallel cables on Mac OS X. 
- The test page did not print the rulers properly on large media sizes (Issue #2252) 
- The text filter could crash when pretty printing certain types of files (Issue #2158)
