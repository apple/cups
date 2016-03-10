---
title: Common UNIX Printing System 1.2.3
layout: post
permalink: /blog/:year-:month-:day-:title.html
---

<P>CUPS 1.2.3 is now available for download from the CUPS web site at:</P><PRE>    <A HREF="http://www.cups.org/software.html">http://www.cups.org/software.html</A></PRE><P>CUPS 1.2.3 fixes a number of web interface, networking, remote printing, and CUPS API issues. Changes include:</P>
- The scheduler did not send job-state or job-config-changed events when a job was held, released, or changed (Issue #1947) 
- The scheduler now aborts if the configuration file and directory checks fail (Issue #1941) 
- Fixed a problem with ippPort() not using the port number that was set via the client.conf file or CUPS_SERVER environment variable (Issue #1945) 
- HTTP headers were not buffered (Issue #1899) 
- Some IPP printers (HP) did not like UTF-8 job names (Issue #1837) 
- The CUPS desktop icon is now localized for Polish (Issue #1920) 
- Printer options were not always honored when printing from Windows clients (Issue #1839) 
- The openssl command would lock up the scheduler when generating an encryption certificate on some platforms due to a lack of entropy for the random number generator (Issue #1876) 
- The web admin page did not recognize that "Listen 631" enabled remote access (Issue #1908) 
- The web admin page did not check whether changes were made to the Basic Server Settings check boxes (Issue #1908) 
- The IPP backend could generate N*N copies in certain edge cases. 
- The scheduler did not restore remote printers properly when BrowseShortNames was enabled (Issue #1893) 
- Polling did not handle changes to the network environment on Mac OS X (Issue #1896) 
- The "make test" subscription tests used invalid notify-recipient-uri values (Issue #1910) 
- Printers could be left in an undefined state on system sleep (Issue #1905) 
- The Berkeley and System V commands did not always use the expected character set (Issue #1915) 
- Remote printing fixes (Issue #1881) 
- The cupstestppd utility did not validate translation strings for custom options properly. 
- Multi-language PPD files were not properly localized in the web interface (Issue #1913) 
- The admin page's simple settings options did not check for local domain socket or IPv6 addresses and did not use "localhost" as the listen address. 
- An empty BrowseProtocols, BrowseLocalProtocols, or BrowseRemoteProtocols line would crash the scheduler instead of disabling the corresponding browsing options. 
- The scheduler now logs IPP operation status as debug messages instead of info or error. 
- cupsFileRewind() didn't clear the end-of-file state. 
- cupstestppd didn't report the actual misspelling of the 1284DeviceID attribute (Issue #1849)  
- BrowseRelay didn't work on Debian (Issue #1887) 
- configure --without-languages didn't work (Issue #1879) 
- Manually added remote printers did not work (Issue #1881) 
- The &lt;cups/backend.h> header was not installed. 
- Updated the build files for Autoconf 2.60 (Issue #1853) 
- The scheduler incorrectly terminated the polling processes after receiving a partial log line. 
- The cups-lpd mini-daemon reported "No printer-state attribute found" errors when reporting the queue status (PR #6250, Issue #1821) 
- SNMP backend improvements (Issue #1737, Issue #1742, Issue #1790, Issue #1835, Issue #1880) 
- The scheduler erroneously reported an error with the CGI pipe (Issue #1860) 
- Fixed HP-UX compile problems (Issue #1858, Issue #1859) 
- cupstestppd crashed with some PPD files (Issue #1864) 
- The &lt;cups/dir.h> and &lt;cups/file.h> header files did not work with C++.
