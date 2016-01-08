---
title: Common UNIX Printing System 1.3rc1
layout: post
---

The first release candidate for CUPS 1.3 is now available for download from:

     http://www.cups.org/software.html

As per the CUPS Configuration Management Plan, we now start our two week "soak" of each release candidate. Once we are happy with the quality, we'll do the first stable release, 1.3.0. If you experience problems with the release candidate, please post your issues to the cups.general forum or mailing list. Confirmed bug reports should be posted to the Bugs &amp; Features page. 
CUPS 1.3 adds Kerberos and mDNS (Bonjour) support along with over 30 new features. Changes in 1.3rc1 include:
- Updated the German localization (Issue #2443)
- cupsAdminGetServerSettings() did not handle &lt;/Foo&gt; properly.
- When lprm and cancel are run with no job ID, they now will cancel the first stopped job if no pending or processing jobs are left in the queue.
- The scheduler now logs successful print jobs, filter failures, and the job file types at the default log level (Issue #2458)
- The scheduler now logs the usernames it is using for authorization at LogLevel debug instead of debug2 (Issue #2448)
- Added Intellitech Intellibar and Zebra CPCL PPDs to the list of installed PPDs.
- Added 6&quot; and 8&quot; wide label sizes for the Zebra ZPL Label Printer driver (Issue #2442)
- The cupsaddsmb program and web interface now support exporting of 64-bit Windows drivers, when available (Issue #2439)
- Moving a job that was printing did not stop the job on the original printer (Issue #2262)
- The cups-lpd mini-daemon did not work on Mac OS X server.
- Added httpGetAuthString() and httpSetAuthString() APIs to get and set the current (cached) authorization string to use for HTTP requests.
- Updated the default cupsd.conf policy to list the &quot;administrative&quot; operations separately from the &quot;printer control&quot; operations so that it is easier to define a group of users that are &quot;printer operators&quot;.
- The web interface now pulls the default cupsd.conf file from cupsd.conf.default in the CUPS config directory.
- Added a help file for using Kerberos with CUPS.
- The scheduler now strips the &quot;@KDC&quot; portion of Kerberos usernames since those usernames typically do not appear in the group membership lists used by CUPS.
- cupsMarkOptions() could (incorrectly) leave multiple option choices marked.
- Backends could (incorrectly) run as root during discovery (Issue #2454)
- Avahi is now supported for DNS-SD (Bonjour) printer sharing (Issue #2455)
- The default cupsd.conf file had typos and old operation names (Issue #2450)
- The scheduler now erases authentication cache files using the 7-pass US DoD algorithm.
- Delegated Kerberos credentials (proxy authentication) did not work.
- The filter makefile did not optimize the libcupsimage.2.dylib with a sectorder file.
- The IPP backend incorrectly wrote an empty printer message when processing the &quot;none&quot; state reason.
- The USB backend could deadlock on Mac OS X while performing a side-channel command.
- The scheduler did not prevent remote queues from being shared/published.
- The scheduler did not remove the temporary request file on authentication errors.
- ppdLocalizeIPPReason() did not handle &quot;scheme:&quot; schemes or &quot;file&quot; URLs.
- ppdLocalizeIPPReason() was not exported on Mac OS X.
