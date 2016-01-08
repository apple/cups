---
title: Common UNIX Printing System 1.2.2
layout: post
---

<P>CUPS 1.2.2 is now available for download from the CUPS web site at:</P><PRE>    <A HREF="http://www.cups.org/software.html">http://www.cups.org/software.html</A></PRE><P>CUPS 1.2.2 fixes several build, platform, notification, and printing bugs. Changes include:</P>
- Documentation updates (Issue #1765, Issue #1780)
- CUPS didn't know about alternate character set names for Asian text (Issue #1819)
- The lpoptions -o and -r options did not work unless you specified a printer.
- The lpoptions command incorrectly allowed users to set printer attributes like printer-type (Issue #1791)
- httpWait() did not flush the write buffer, causing "bad request" errors when communicating with CUPS 1.1.x servers (Issue #1717)
- Polling did not sanitize the printer description, location, or make and model strings like broadcasts did.
- Polled printers did not show the server's default job-sheets option value.
- The Samba password prompt was not properly localized (Issue #1814)
- Added a German translation (Issue #1842)
- The scheduler now creates self-signed SSL certficates automatically when using OpenSSL and CDSA for encryption, just as for GNU TLS.
- The SNMP backend sporatically reported some printers as "unknown" (Issue #1774)
- The scheduler now forces BrowseTimeout to be at least twice the BrowseInterval value and non-zero to avoid common configuration errors.
- The scheduler incorrectly returned printer URIs of the form "ipp://server/printers/classname" for classes (Issue #1813)
- Updated Japanese localization (Issue #1805)
- The scheduler's SSL certificate/key directory was not created on installation (Issue #1788)
- Added a mailto.conf man page and help page (Issue #1754)
- The parallel and USB backends no longer wait for the printer to go on-line - this caused problems with certain printers that don't follow with the IEEE-1284 standard (Issue #1738)
- The scheduler could crash on a reload when implicit classes were present (Issue #1828)
- The IPP backend incorrectly used the CUPS_ENCRYPTION environment variable to determine the default encryption mode when printing (Issue #1820)
- USB printing did not work on Solaris (Issue #1756)
- The scheduler sorted job priorities in the wrong order (Issue #1811)
- The scheduler did not automatically restart notifiers that exited or crashed (Issue #1793)
- IPv6 support did not work on NetBSD (Issue #1834)
- The EPM packaging file did not work (Issue #1804)
- The scheduler used up the CPU if BrowseRemoteProtocols was empty (Issue #1792)
- Custom page sizes did not work (Issue #1787)
- The SNMP backend could crash on some systems when SNMP logging was enabled (Issue #1789)
- Browsing could produce some funny printer names when ServerName was set to an IP address (Issue #1799)
- Fixed the log message for BrowseRelay (Issue #1798)
- Fixes to allow CUPS to compile on MirBSD (Issue #1796)
- The scheduler incorrectly set the FINAL_CONTENT_TYPE environment variable (Issue #1795)
- The pdftops filter incorrectly embedded a "produced by" comment, causing PDF printing not to work on some operating systems (Issue #1801)
- Sending raw jobs from a client system could cause the client's scheduler to eventually crash (Issue #1786)
- The scheduler now checks that the notifier exists prior to accepting a new subscription request.
- The scheduler now reports the supported notify-recipient schemes based on the contents of the ServerBin/notifier directory.
- Event notifications did not include the notify-sequence-number or other required attributes (Issue #1747)
- Allow/Deny addresses of the form "11.22.33.*" did not work on Linux (Issue #1769)
- cupsGetPPD() did not work if the scheduler was only listening on a domain socket (Issue #1766)
- The scheduler could crash advertising a class (Issue #1768)
- The scheduler could crash if the default printer was deleted (Issue #1776)
- Added a new default CUPS raster format (v3) which does not compress the raster stream in order to provide the same cupsRasterReadPixels() and cupsRasterWritePixels() performance as CUPS 1.1.x.
- The cupsaddsmb man page listed the wrong files for the CUPS driver.
- Some configure --with options did not work (Issue #1746)
- "Allow @IF(name)" didn't work if "name" wasn't the first network interface (Issue #1758)
- The lpstat command did not use the correct character set when reporting the date and time (Issue #1751)
- The cupsaddsmb command and web interface did not update the Windows PPD files properly, resulting in corrupt PPD files for the Windows client to use (Issue #1750)
- The cupsd.conf man page didn't describe the Listen domain socket syntax (Issue #1753)
- The scheduler no longer tries to support more than FD_SETSIZE file descriptors.
- CDSA (encryption) support fixes for MacOS X.
- The lppasswd program needs to be setuid to root to create and update the /etc/cups/passwd.md5 file (Issue #1735)
- 32/64-bit library installation was broken (Issue #1741)
- The USB backend now reports a "no such device" error when using the old filename-based USB URIs instead of the "success" error.
- Increased the HTTP and IPP read timeouts to 10 seconds, as 1 second was too short on congested networks (Issue #1719)
- The SNMP backend now uses the device description over the printer-make-and-model attribute when the attribute contains a generic name (Issue #1728)
- Fixed another file descriptor leak when printing raw files (Issue #1736)
- Raw queues were not shared via LDAP (Issue #1739)
- The pstops filter didn't always embed PageSetup commands from the PPD file (Issue #1740)
- "make install" didn't work if you disabled all of the localizations.
- The scheduler didn't always choose the least costly filter.
- Fixed parsing of IPv6 addresses in Allow, Deny, BrowseAllow, BrowseDeny, and BrowseRelay directives (Issue #1713)
- Printers that were shared via LDAP did not get added to the LDAP server properly (Issue #1733)
- LDAP browsing would crash the scheduler if a required value was missing (Issue #1731)
- Special cases for the "localhost" hostname did not work, causing printing to not work when the /etc/hosts file did not contain a localhost entry (Issue #1723)
- Updated the Spanish translation (Issue #1720, Issue #1770)
- Reverse-order page output was broken when N-up or landscape orientations were used (Issue #1725)
- The parallel, serial, socket, and USB backends needed print data before they would report back-channel data, causing problems with several new drivers (Issue #1724)
