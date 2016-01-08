---
title: Common UNIX Printing System 1.2.0
layout: post
---

<P>CUPS 1.2.0 is now available for download from the CUPS web site at:</P><PRE>    <A HREF="http://www.cups.org/software.html">http://www.cups.org/software.html</A></PRE><P>CUPS 1.2.0 is the first stable feature release in the 1.2.x series and includes over 90 new features and changes since CUPS 1.1.23, including a greatly improved web interface and "plug-and-print" support for many local and network printers. For a complete list of changes and new features, please consult the "What's New in CUPS 1.2" document at:</P><PRE>    <A HREF="http://www.cups.org/documentation.php/whatsnew.html">http://www.cups.org/documentation.php/whatsnew.html</A></PRE><P>CUPS provides a portable printing layer forUNIX&reg;-based operating systems. It has beendeveloped by <A HREF="http://www.easysw.com/">Easy SoftwareProducts</A> to promote a standard printing solution for allUNIX vendors and users. CUPS provides the System V and Berkeleycommand-line interfaces.</P><P>CUPS uses the Internet Printing Protocol ("IPP") as the basisfor managing print jobs and queues. The Line Printer Daemon("LPD") Server Message Block ("SMB"), and AppSocket (a.k.a.JetDirect) protocols are also supported with reducedfunctionality. CUPS adds network printer browsing and PostScriptPrinter Description ("PPD") based printing options to supportreal-world printing under UNIX.</P><P>CUPS includes an image file RIP that supports printing ofimage files to non-PostScript printers.  A customized version ofGNU Ghostscript 8.15 for CUPS called ESP Ghostscript isavailable separately to support printing of PostScript fileswithin the CUPS driver framework.  Sample drivers for Dymo,EPSON, HP, OKIDATA, and Zebra printers are included that usethese filters.</P><P>Drivers for thousands of printers are provided with our ESPPrint Pro software, available at:<PRE>    <A HREF="http://www.easysw.com/printpro/">http://www.easysw.com/printpro/</A></PRE><P>CUPS is licensed under the GNU General Public License and GNULibrary General Public License.  Please contact<A HREF="mailto:info@easysw.com">Easy Software Products</A> forcommercial support and "binary distribution" rights.<P>Changes in CUPS 1.2.0 since 1.2rc3:</P><UL>
- Documentation updates (Issue #1618, Issue #1620, Issue #1622, Issue #1637) 
- Static file copy buffers reduced from 64k to 32k to work around bogus MallocDebug library assumptions (Issue #1660) 
- The scheduler did not decode the backend exit code properly (Issue #1648) 
- The MacOS X USB backend did not report the 1284 device ID, nor did it fix device IDs returned by HP printers. 
- The scheduler started more slowly than 1.1.x with large numbers of printers (Issue #1653) 
- cupsRasterInterpretPPD() didn't support the cupsPreferredBitsPerColor attribute, and imagetoraster didn't use the new API. 
- The "make test" script did not create all of the necessary subdirectories for testing (Issue #1638) 
- The scheduler did not prevent rotation of logs redirected to /dev/null (Issue #1651) 
- "make test" did not include the SNMP backend in the test environment (Issue #1625) 
- The EPM packaging files did not work (Issue #1621) 
- "Use Default Configuration" inserted a broken configuration file (Issue #1624) 
- Redirects in the web interface did not always preserve the encrypted status of a connection (Issue #1603) 
- Added the Apple "pap" backend. 
- Added CUPS library to CUPS Image shared library linkage to support Linux --as-needed linker option (Issue #1606) 
- Fixed support for --enable-pie (Issue #1609) 
- The pdftops filter did not validate the length of the encryption key (Issue #1608) 
- Updated the Polish localization. 
- "Encryption Required" in the cupsd.conf file now only requires encryption when the connection is not over the loopback interface or domain socket. 
- Printer names containing "+" were not quoted properly in the web interface (Issue #1600) 
- The SNMP backend now reports the make and model in the information string so that the auto-generated printer name is more useful than just an IP address. </UL>
