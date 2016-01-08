---
title: Common UNIX Printing System 1.2.1
layout: post
---

<P>CUPS 1.2.1 is now available for download from the CUPS web site at:</P><PRE>    <A HREF="http://www.cups.org/software.html">http://www.cups.org/software.html</A></PRE><P>CUPS 1.2.1 fixes several build, platform, and printing bugs. Changes include:</P>
- The web interface did not handle reloads properly for MSIE (Issue #1716) 
- The configure script no longer adds linker rpath options when they are unnecessary. 
- The scheduler could crash printing a debug message on Solaris (Issue #1714) 
- The --enable-32bit and --enable-64bit configure options did not always work. 
- The password prompt showed the domain socket address instead of "localhost" for local authentication (Issue #1706) 
- The web interface filtered the list of printers even if the user wasn't logged in (Issue #1700) 
- The IPP backend did not work reliably with some Xerox printers (Issue #1704) 
- Trailing banners were not added when printing a single file (Issue #1698) 
- The web interface support programs crashed on Solaris (Issue #1699) 
- cupstestppd incorrectly reported problems with &#x2a;1284DeviceID attributes (Issue #1710) 
- Browsing could get disabled after a restart (Issue #1670) 
- Custom page sizes were not parsed properly (Issue #1709) 
- The -U option wasn't supported by lpadmin (Issue #1702) 
- The -u option didn't work with lpadmin (Issue #1703) 
- The scheduler did not create non-blocking back-channel pipes, which caused problems when the printer driver did not read the back-channel data (Issue #1705) 
- The scheduler no longer uses chunking in responses to clients - this caused problems with older versions of CUPS like 1.1.17 (PR #6143) 
- Automatic raw printing was broken (Issue #1667) 
- 6-up printing was broken (Issue #1697) 
- The pstops filter did not disable CTRL-D processing on the printer/RIP. 
- ppdOpen&#x2a;() did not load custom options properly (Issue #1680) 
- "Set Printer Options" in the web interface did not update the DefaultImageableArea or DefaultPaperDimension attributes in the PPD file (Issue #1689) 
- Fixed compile errors (Issue #1682, Issue #1684, Issue #1685, Issue #1690) 
- The lpstat command displayed the wrong error message for a missing destination (Issue #1683) 
- Revised and completed the Polish translation (Issue #1669) 
- Stopped jobs did not show up in the list of active jobs (Issue #1676) 
- The configure script did not use the GNU TLS "libgnutls-config" script to find the proper compiler and linker options. 
- The imagetoraster filter did not correctly generate several 1, 2, and 4-bit color modes. 
- cupsRasterWritePixels() could lose track of the current output row. 
- cupsRasterReadPixels() did not automatically swap 12/16-bit chunked pixel data. 
- Moved the private _cups_raster_s structure out of the public header. 
- Updated the CUPS raster format specification to include encoding rules and colorspace definitions. 
- The Zebra PPD files had the wrong PostScript code for the "default" option choices. 
- The imagetoraster filter did not generate correct CIE XYZ or Lab color data. 
- The cups-config script did not work when invoked from a source directory (Issue #1673) 
- The SNMP backend did not compile on systems that used the getifaddrs emulation functions (Issue #1668)
