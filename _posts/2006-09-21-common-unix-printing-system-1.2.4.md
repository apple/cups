---
title: Common UNIX Printing System 1.2.4
layout: post
---

<P>CUPS 1.2.4 is now available for download from the CUPS web site at:</P><PRE>    <A HREF="http://www.cups.org/software.html">http://www.cups.org/software.html</A></PRE><P>CUPS 1.2.4 fixes a number of web interface, scheduler, and CUPS API issues. Changes include:</P>
- The --with-printcap configure option did not work (Issue #1984) 
- The character set reported by cupsLangGet() did not always reflect the default character set of a given locale (Issue #1983) 
- Older Lexmark and Tektronix printers did not work with IPP (Issue #1980) 
- Failsafe printing did not work (PR #6328) 
- Some web interface redirects did not work (Issue #1978) 
- The web interface change settings button could introduce a "Port 0" line in cupsd.conf if there was no loopback connection available (Issue #1979) 
- The web interface change settings and edit configuration file buttons would truncate the cupsd.conf file (Issue #1976) 
- The German web interface used the wrong printer icon images (Issue #1973) 
- The "All Documents" link in the on-line help was missing a trailing slash (Issue #1971) 
- The Polish web interface translation used the wrong URLs for the job history (Issue #1963) 
- The "reprint job" button did not work (Issue #1956) 
- The scheduler did not always report printer or job events properly (Issue #1955) 
- The scheduler always stopped the queue on error, regardless of the exit code, if the error policy was set to "stop-printer" (Issue #1959) 
- ppdEmitJCL() included UTF-8 characters in the JCL job name, which caused problems on some printers (Issue #1959) 
- Fixed a buffering problem that cause high CPU usage (Issue #1968) 
- The command-line applications did not convert command-line strings to UTF-8 as needed (Issue #1958) 
- cupsDirRead() incorrectly aborted when reading a symbolic link that pointed to a file/directory that did not exist (Issue #1953) 
- The cupsInterpretRasterPPD() function did not handle custom page sizes properly.
