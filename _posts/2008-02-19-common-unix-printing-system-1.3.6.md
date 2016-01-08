---
title: Common UNIX Printing System 1.3.6
layout: post
---

CUPS 1.3.6 is now available for download from the CUPS web site:    http://www.cups.org/software.htmlThe new release fixes some platform-specific build problems, web interface issues, PDF and PostScript filter option handling, and a number of minor bugs discovered during routine code audits.Changes include:
- Documentation updates (Issue #2646, Issue #2647, Issue #2649)
- Fixed a problem with the web interface &quot;Use Kerberos Authentication&quot; check box (Issue #2703)
- The scheduler unconditionally overwrote the printer-state- message with &quot;process-name failed&quot; when a filter or backend failed, preventing a useful error message from being shown to the user.
- Policies on CUPS-Move-Job didn't work as expected (Issue #2699)
- The configure script only supported D-BUS on Linux (Issue #2702)
- The scheduler did not support &lt;/LimitExcept&gt; (Issue #2701)
- The scheduler did not reset the job-hold-until attribute after a job's hold time was reached.
- The scheduler did not support printer supply attributes (Issue #1307)
- The Kerberos credentials provided by some Windows KDCs were still too large - now use a dynamic buffer to support credentials up to 64k in size (Issue #2695)
- Printing a test page from the web interface incorrectly defaulted to the &quot;guest&quot; user (Issue #2688)
- The cupsEncodeOptions2() function did not parse multiple- value attribute values properly (Issue #2690)
- The scheduler incorrectly sent printer-stopped events for status updates from the print filters (Issue #2680)
- The IPP backend could crash when handling printer errors (Issue #2667)
- Multi-file jobs did not print to remote CUPS servers (Issue #2673)
- The scheduler did not provide the Apple language ID to job filters.
- Kerberos authentication did not work with the web interface (Issue #2606, Issue #2669)
- The requesing-user-name-allowed and -denied functionality did not work for Kerberos-authenticated usernames (Issue #2670)
- CUPS didn't compile on HP-UX 11i (Issue #2679)
- cupsEncodeOptions2() did not handle option values like &quot;What's up, doc?&quot; properly.
- Added lots of memory allocation checks (Fortify)
- The scheduler would crash if it was unable to add a job file (Fortify)
- ppdOpen*() did not check all memory allocations (Coverity)
- ippReadIO() did not check all memory allocations (Coverity)
- The PostScript filter did not detect read errors (Coverity)
- The scheduler did not check for a missing job-sheets-completed attribute when sending an event notification (Coverity)
- &quot;Set Printer Options&quot; might not work with raw queues (Coverity)
- cupsRasterInterpretPPD() could crash on certain PostScript errors (Coverity)
- The USB backend did not check for back-channel support properly on all systems (Coverity)
- Fixed memory leaks in the GIF and PNM image loading code (Coverity)
- Removed some dead code in the CUPS API and scheduler (Coverity)
- Fixed two overflow bugs in the HP-GL/2 filter (Coverity)
- Fixed another ASN1 string parsing bug (Issue #2665)
- The RSS notifier directory was not installed with the correct permissions.
- The standard CUPS backends could use 100% CPU while waiting for print data (Issue #2664)
- Filename-based MIME rules did not work (Issue #2659)
- The cups-polld program did not exit if the scheduler crashed (Issue #2640)
- The scheduler would crash if you tried to set the port-monitor on a raw queue (Issue #2639)
- The scheduler could crash if a polled remote printer was converted to a class (Issue #2656)
- The web interface and cupsctl did not correctly reflect the &quot;allow printing from the Internet&quot; state (Issue #2650)
- The scheduler incorrectly treated MIME types as case- sensitive (Issue #2657)
- The Java support classes did not send UTF-8 strings to the scheduler (Issue #2651)
- The CGI code did not handle interrupted POST requests properly (Issue #2652)
- The PostScript filter incorrectly handled number-up when the number of pages was evenly divisible by the number-up value.
- The PDF filter incorrectly filtered pages when page-ranges and number-up were both specified (Issue #2643)
- The IPP backend did not handle printing of pictwps files to a non-Mac CUPS server properly.
- The scheduler did not detect network interface changes on operating systems other than Mac OS X (Issue #2631)
- The scheduler now logs the UNIX error message when it is unable to create a request file such as a print job.
- Added support for --enable-pie on Mac OS X.
