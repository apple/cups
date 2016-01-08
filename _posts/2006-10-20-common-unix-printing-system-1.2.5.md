---
title: Common UNIX Printing System 1.2.5
layout: post
---

<P>CUPS 1.2.5 is now available for download from the CUPS web site at:</P><PRE>    <A HREF="http://www.cups.org/software.html">http://www.cups.org/software.html</A></PRE><P>CUPS 1.2.5 fixes minor printing, networking, and documentation issues and adds support for older versions of DBUS and a translation for Estonian. Changes include:</P>
- Documentation updates (Issue #2038) 
- The SNMP backend no longer uses IPP for Epson printers (Issue #2028) 
- Updated the configure script for Tru64 UNIX 5.1 (Issue #2033) 
- Tru64 5.1B's getaddrinfo() and getnameinfo() functions leak file descriptors (Issue #2034) 
- cupsAddDest() didn't add the parent destination's options and attributes. 
- ppdConflicts() did not handle custom option constraints. 
- Raw printing of gzip'd files did not work (Issue #2009) 
- The scheduler no longer preserves default option choices when the new PPD no longer provides the old default choice (Issue #1929) 
- The Linux SCSI backend is now only built if the SCSI development headers are installed. 
- USB printing to Minolta printers did not work (Issue #2019) 
- Windows clients could not monitor the queue status (Issue #2006) 
- The scheduler didn't log the operation name in the access_log file for Create-Job and Print-Job requests. 
- The PostScript filter now separates collated copies with any required JCL commands so that JCL-based finishing options act on the individual copies and not all of the copies as a single document. 
- The PostScript filter now disables duplex printing when printing a 1-page document. 
- cups-lpd didn't pass the correct job-originating-host-name value (Issue #2023) 
- Fixed some speling errors in the German message catalog (Issue #2012) 
- cupstestppd did not catch PPD files with bad UIConstraints values (Issue #2016) 
- The USB backend did not work with the current udev- created printers if the first printer was disconnected (Issue #2017) 
- Mirrored and rotated printing did not work with some documents (Issue #2004) 
- 2-sided printing with banners did not work properly on some printers (Issue #2018) 
- Updated the raw type rule to handle PJL within the first 4k of a print job (Issue #1969) 
- Added an Estonian translation (Issue #1957) 
- Clarified the documentation for the cupsd.conf @LOCAL and @IF(name) allow/deny functionality (Issue #1992) 
- The PostScript filters did not escape the Title and For comments in the print job header (Issue #1988) 
- The scheduler would use 100% CPU if browsing was disabled and the cupsd.conf file contained BrowsePoll lines (Issue #1994) 
- The cupsDirRead() function did not work properly on non-POSIX-compliant systems (Issue #2001) 
- The cupsFile functions didn't handle read/write errors properly (Issue #1996) 
- The DBUS support now works with older versions of the DBUS library.
