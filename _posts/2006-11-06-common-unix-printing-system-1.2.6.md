---
title: Common UNIX Printing System 1.2.6
layout: post
permalink: /blog/:year-:month-:day-:title.html
---

<P>CUPS 1.2.6 is now available for download from the CUPS web site at:</P><PRE>    <A HREF="http://www.cups.org/software.html">http://www.cups.org/software.html</A></PRE><P>CUPS 1.2.6 fixes some compile errors, localization of the web interface on Mac OS X, bugs in the lpc and lpstat commands, and backchannel support in the parallel backend. Changes include:</P>
- The web interface was not localized on Mac OS X (<A HREF="http://www.cups.org/str.php?L2075">Issue #2075</A>) 
- "lpc status" did not show the number of queued jobs for disabled queues (<A HREF="http://www.cups.org/str.php?L2069">Issue #2069</A>) 
- The lpstat program could hang (<A HREF="http://www.cups.org/str.php?L2073">Issue #2073</A>) 
- The serial backend did not support the new USB serial filenames on Linux (<A HREF="http://www.cups.org/str.php?L2061">Issue #2061</A>) 
- The parallel backend did not support bidirectional I/O properly (<A HREF="http://www.cups.org/str.php?L2056">Issue #2056</A>) 
- The network backends now log the numeric address that is being used (<A HREF="http://www.cups.org/str.php?L2046">Issue #2046</A>) 
- Fixed a compile error when using libpaper. 
- Fixed a compile error when compiling on Solaris with threading enabled (<A HREF="http://www.cups.org/str.php?L2049">Issue #2049</A>, <A HREF="http://www.cups.org/str.php?L2050">Issue #2050</A>) 
- Missing printer-state-changed event for printer-state-message updates (<A HREF="http://www.cups.org/str.php?L2047">Issue #2047</A>)
