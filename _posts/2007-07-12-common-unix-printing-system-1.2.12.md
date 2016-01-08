---
title: Common UNIX Printing System 1.2.12
layout: post
---

<P>CUPS 1.2.12 is now available for download from the CUPS web site at:</P><PRE>    <A HREF="http://www.cups.org/software.html">http://www.cups.org/software.html</A></PRE><P>CUPS 1.2.12 fixes several file typing issues, a bad error message in the scheduler, a web interface setting problem, and a bug in the PHP language binding. It also includes an updated Italian translation. Changes include:</P>
- The PHP cups_print_file() function crashed if the options array contained non-string option values (Issue #2430) 
- The image/tiff file matching rule incorrectly identified some text files as TIFF files (Issue #2431) 
- The filter(7) man page incorrectly documented the "PAGE: total #-pages" message (Issue #2427) 
- PCL text files were mis-identified as HP-GL/2 and caused the HP-GL/2 filter to hang (Issue #2423) 
- When printing to a queue with user ACLs, the scheduler incorrectly returned a quota error instead of a "not allowed to print" error (Issue #2409) 
- cupsaddsmb could get in a loop if no printer drivers were installed (Issue #2407) 
- cupsRasterReadHeader() did not byte-swap the header properly when compiled with certain versions of GCC. 
- The IPP backend did not send the document-format attribute for filtered jobs (Issue #2411) 
- Some PPD files could cause a crash in ppdOpen2 (Issue #2408) 
- The web admin interface incorrectly handled the "share printers" and "show remote printers" settings (Issue #2393) 
- The scheduler's log messages about AuthClass and AuthGroupName advised using a replacement directive but had the wrong syntax (Issue #2400) 
- Updated the PostScript/PJL and HP-GL/2 MIME rules to look in the first 4k of the file, not just the first 1k (Issue #2386) 
- Updated the Italian localization (Issue #2382)
