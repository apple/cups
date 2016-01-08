---
title: Common UNIX Printing System 1.1.21rc1
layout: post
---

<P>The first release candidate for version 1.1.21 of the CommonUNIX Printing System ("CUPS") is now available for download fromthe CUPS web site at:<PRE>    <A HREF="http://www.cups.org/software.html">http://www.cups.org/software.html<P>In accordance with the CUPS Configuration Management Plan,you now have until Thursday, June 10th to test this releasecandidate to determine if there are any high-priority problemsand report them using the Software Trouble Report form at:<PRE>    <A HREF="http://www.cups.org/str.php">http://www.cups.org/str.php<P>Reports sent to the CUPS newsgroups or mailing lists are notautomatically entered into the trouble report database and willnot influence the final production release of 1.1.21, so it isvery important that you report any problems you identify usingthe form.<P>CUPS 1.1.21 is primarily a bug fix and performance tuningrelease and includes fixes for the IPP, LPD, parallel, serial,and USB backends, authentication and status processing issues inthe CUPS API, and various PostScript and PDF printing issues.The new release also adds support for Zebra label printers andIPP device URI options.<P>Changes in CUPS v1.1.21rc1:<UL>	
- Fixed some "type-punned" warnings produced by GCC when 	-fstrict-aliasing is specified (Issue #679) 	
- The PDF filter incorrectly calculated the bounding box 	of a page (Issue #682) 	
- The IPP backend did not use SSL when printing over a 	port other than 443 (Issue #730) 	
- The scheduler could crash when processing a Limit or 	LimitExcept directive (Issue #728) 	
- The lpq, lpr, and lp commands did not differentiate 	between the server being unresponsive and the lack of 	a default printer (Issue #728) 	
- The PAM checks in the configure script did not stop 	after the first match (Issue #728) 	
- The cups-config man page was incorrectly placed in 	section 3 (Issue #728) 	
- The cupstestppd utility did not show a warning message 	when a PPD file indicated BCP protocol support with 	PJL (Issue #720) 	
- The scheduler did not return the correct exit code 	when startup failed (Issue #718) 	
- The cupsRasterReadPixels() function checked for 	EAGAIN, which caused problems on FreeBSD (Issue #723) 	
- The cupsGetDests() function did not use the current 	encryption setting (Issue #653) 	
- The scheduler did not properly parse name-based 	BrowseRelay directives in the cupsd.conf file (STR 	#711) 	
- The IPP backend now supports the following options in 	the device URI: encryption, waitjob, and waitprinter 	(Issue #699) 	
- The parallel, serial, socket, and USB backends did not 	return a non-zero exit status when a job failed to 	print in the middle of sending it (Issue #715) 	
- Location directives in the cupsd.conf file were 	case-sensitive for printer and class names, so 	queue-specific access control was not reliable (STR 	#700) 	
- cupsDoFileRequest() did not handle HTTP continue 	status messages in all cases, causing sporatic 	problems with IPP printers from some vendors (STR 	#716) 	
- The rastertodymo driver now supports the Zebra ZPL 	language (Issue #713) 	
- The test suite no longer generates a printcap file, 	which caused problems when testing as the root user 	(Issue #693) 	
- The scheduler now updates the accepting state of an 	implicit class based upon the accepting state of its 	member printers (Issue #697) 	
- The pstops filter didn't properly skip leading PJL 	commands (Issue #664) 	
- The reinterpret_cast keyword was not highlighted when 	printing C/C++ source files in prettyprint mode (STR 	#694) 	
- Fixed a segfault problem with some of the client 	programs (Issue #668) 	
- When using RunAsUser, the scheduler did not correctly 	set the ownership of the log files, preventing log 	file rotation (Issue #686) 	
- The image filters did not correctly load 1-bit PNG 	files (Issue #687) 	
- The pdftops filter did not show all annotation objects 	in a PDF file (Issue #674) 	
- The pdftops filter did not print the contents of 	textual form elements, making it impossible to print a 	filled-in form (Issue #663) 	
- Integrated the MacOS X/Darwin USB backend into the 	CUPS baseline (Issue #661) 	
- The USB backend incorrectly reported "media tray 	empty" (Issue #660) 	
- The scheduler did not use a case-insensitive 	comparison when checking for group membership, which 	caused problems with Win9x clients printing via SAMBA 	(Issue #647) 	
- The scheduler did not report the addresses associated 	with certain network errors, making troubleshooting 	difficult (Issue #648, #649) 	
- The cupstestppd program did not allow a default choice 	of "Unknown" as required by the PPD spec (Issue #651) 	
- The select() buffers are now allocated to be at least 	as large as sizeof(fd_set) (Issue #639) 	
- The LPD backend now supports overriding the print job 	username via the device URI (Issue #631) 	
- The scheduler did not handle an unknown MIME type when 	checking for a CGI script (Issue #603) 	
- Added a timeout optimization to the scheduler's main 	loop to allow CUPS to sleep more of the time (STR 	#629) 	
- The USB backend now retries printing to devices of the 	form "usb://make/model" if any USB port shows up as 	"busy" (Issue #617) 	
- The httpGetHostByName() function did not range check 	IP address values (Issue #608) 	
- The httpUpdate() function could return HTTP_ERROR 	instead of the HTTP status if the server closed the 	connection before the client received the whole 	response (Issue #611) 	
- The LPD mini-daemon did not allow the administrator to 	force banner pages on (Issue #605) 	
- Added PAM support for Darwin/MacOS X (Issue #550) 	
- The web interface now provides a "Set As Default" 	button to set the default printer or class on a server 	(Issue #577) 	
- The HTTP authentication cache was broken (Issue #517) 	
- The cupstestppd utility now fails PPD files that have 	a DefaultOption keyword for a non-existance option 	name (Issue #476) 	
- Optimized the scanning of new PPD files on scheduler 	startup (Issue #424) 	
- The EPM list file did not include the bin, lib, or 	sbin directories (Issue #598) 	
- The web interface did not redirect administration 	tasks to the primary server for a class or printer 	(Issue #491, Issue #652) 	
- The cups-lpd mini-daemon did not reject print jobs to 	queues that were rejecting new print jobs (Issue #515) 	
- Some calls to the ctype functions did not account for 	platforms that use a signed char type by default (STR 	#518) 	
- The scheduler could use excess amounts of CPU if a CGI 	program was sending data faster than the client could 	take it (Issue #595) 	
- Updated the Ghostscript 8.x integration stuff (STR 	#484) 	
- The lpd backend used a source port of 732 by default, 	which is outside of the range defined by RFC 1179; 	also added a new (default) "reserve=any" option for 	any priviledged port from 1 to 1023 (Issue #474) 	
- The scheduler did not check for a valid Listen/Port 	configuration (Issue #499) 	
- The cupsPrintFiles() function did not always set the 	last IPP error message (Issue #538) 	
- The pstops filter did not write the PostScript header 	line if the file began with a PJL escape sequence (STR 	#574) 	
- The printer-is-accepting-jobs status of remote 	printers was not sent to clients via browsing or 	polling (Issue #571) 	
- The web interface did not show the printer state 	history information (Issue #592) 	
- The rastertoepson filter would crash under certain 	cirsumstances (Issue #583) 	
- The USB backend did not handle serial numbers using 	the (incorrect) SN keyword and did not terminate the 	make and model name strings properly (Issue #471, 	Issue #588) 	
- The USB backend did not build on Solaris x86 (STR 	#585) 	
- The cupsDoAuthentication() function did not use the 	method name for Digest authentication (Issue #584) 	
- The scheduler could crash if a print job could not be 	printed and the PreserveJobHistory option was turned 	off (Issue #535) 	
- cups-lpd now logs the temporary filenames that could 	not be opened in order to make troubleshooting easier 	(Issue #565) 	
- cupsGetJobs() now returns -1 on error (Issue #569) 	
- Added localization for Belarusian (Issue #575) 	
- The LPD backend used the full length of the hostname 	when creating the data and control filenames, which 	causes problems with older systems that can't handle 	long filenames (Issue #560) 	
- The scheduler did not refresh the common printer data 	after a fast reload; this prevented banner and other 	information from being updated (Issue #562) 	
- The scheduler did not send common or history data to 	the client when processing a CUPS-Get-Default request 	(Issue #559) 	
- The httpFlush() function did not always flush the 	remaining response data in requests (Issue #558) 	
- The scheduler could complete a job before it collected 	the exit status from all filters and the backend (STR 	#448) 	
- The PPD conformance tests did not catch group 	translation strings that exceeded the maximum allowed 	size (Issue #454) 	
- Updated the client code in the scheduler to close the 	client connection on errors rather than shutting down 	the receive end of the socket; this caused resource 	problems on some systems (Issue #434) 	
- cups-polld didn't compile on Tru64 5.1B (Issue #436) 	
- "lpc stat" crashed if the device URI was empty (STR 	#548) 	
- The scheduler did not compile without zlib (Issue #433) 	
- std:floor() cast needed on IRIX 6.5 with SGI C++ 	compiler (Issue #497) 	
- cupsRasterReadPixels() and cupsRasterWritePixels() did 	not handle EAGAIN and EINTR properly (Issue #473) 	
- RequiresPageRegion should not be consulted for Manual 	Feed (Issue #514) 	
- International characters were not substituted in 	banner files properly (Issue #468) 	
- Updated pdftops to Xpdf 2.03 code to fix printing bugs 	(Issue #470) 	
- The Digest authentication code did not include the 	(required) "uri" attribute in the Authorization 	response, preventing interoperation with Apache 	(Issue #408) 	
- The web interface could lockup when displaying certain 	URLs (Issue #459) 	
- The PostScript filters now convert underscores ("_") 	to spaces for custom classification names (Issue #555)
