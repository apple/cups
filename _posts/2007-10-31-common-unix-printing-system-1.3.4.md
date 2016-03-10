---
title: Common UNIX Printing System 1.3.4
layout: post
permalink: /blog/:year-:month-:day-:title.html
---

CUPS 1.3.4 is now available for download from the CUPS web site and includes fixes for a buffer overflow bug along with some localization, authentication, and printing bugs:
- Documentation updates (Issue #2560, Issue #2563, Issue #2569)
- CUPS now maps the &quot;nb&quot; locale to &quot;no&quot; on all platforms (Issue #2575)
- CUPS did not work with a Windows 2003 R2 KDC (Issue #2568)
- ippReadIO() could read past the end of a buffer (Issue #2561)
- The scheduler would crash on shutdown if it was unable to create a Kerberos context.
- Multiple AuthTypes in cupsd.conf did not work (Issue #2545)
- The snmp.conf file referenced the wrong man page (Issue #2564)
- The cupsaddsmb program didn't handle domain sockets properly (Issue #2556)
- The scheduler now validates device URIs when adding printers.
- Updated httpSeparateURI() to support hostnames with the backslash character.
- Updated the Japanese localization (Issue #2546)
- The parallel backend now gets the current IEEE-1284 device ID string on Linux (Issue #2553)
- The IPP backend now checks the job status at variable intervals (from 1 to 10 seconds) instead of every 10 seconds for faster remote printing (Issue #2548)
- &quot;lpr -p&quot; and &quot;lpr -l&quot; did not work (Issue #2544)
- Compilation failed when a previous version of CUPS was installed and was included in the SSL include path (Issue #2538)
- The scheduler did not reject requests with charsets other than US-ASCII or UTF-8, and the CUPS API incorrectly passed the locale charset to the scheduler instead of UTF-8 (Issue #2537)
- cups-deviced did not filter out duplicate devices.
- The AppleTalk backend incorrectly added a scheme listing when AppleTalk was disabled or no printers were found.
- The PostScript filter generated N^2 copies when the printer supported collated copies and user requested reverse-order output.
- The scheduler did not reprint all of the files in a job that was held.
- The scheduler did not update the printcap file after removing stale remote queues.
- The cupsd.conf man page incorrectly referenced &quot;AuthType Kerberos&quot; instead of &quot;AuthType Negotiate&quot;.
