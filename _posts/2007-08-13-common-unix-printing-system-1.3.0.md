---
title: Common UNIX Printing System 1.3.0
layout: post
permalink: /blog/:year-:month-:day-:title.html
---

CUPS 1.3.0 is now available for download from:

     http://www.cups.org/software.html
<P>CUPS 1.3.0 is the first stable feature release in the 1.3.x series andincludes over 30 new features and changes since CUPS 1.2.12, includingKerberos authentication, DNS-SD/Bonjour/Zeroconf support, improved on-linehelp, and localized printer drivers. For a complete list of changes and newfeatures, please consult the "What's New in CUPS 1.3" document at:</P><PRE>    <A HREF="http://www.cups.org/documentation.php/whatsnew.html">http://www.cups.org/documentation.php/whatsnew.html</A></PRE><P>Changes in CUPS 1.3.0 since 1.3rc2:</P>
- The scheduler did not handle out-of-file conditions gracefully when accepting new connections, leading to heavy CPU usage.
- The scheduler did not detect ServerBin misconfigurations (Issue #2470)
- &quot;AuthType Default&quot; did not work as expected when the &quot;DefaultAuthType foo&quot; line appeared after it in the cupsd.conf file.
- The on-line help did not describe many common printing options (Issue #1846)
- The IPP backend did not return the &quot;auth required&quot; status when printing to a Kerberos-protected queue.
- The scheduler was not looking in the correct directories for LSB PPD files (Issue #2464)
- Changed references to ESP Ghostscript to GPL Ghostscript (Issue #2463)
- The PostScript filter did not cleanly terminate when the job was canceled or stopped.
- Fixed generation of Kerberos credentials for remote printing.  Note that this requires a recent version of MIT Kerberos with a working krb5_cc_new_unique() function or Heimdal Kerberos.
- Added Portuguese and updated Italian message catalogs.
