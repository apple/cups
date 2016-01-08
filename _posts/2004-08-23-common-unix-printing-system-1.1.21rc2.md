---
title: Common UNIX Printing System 1.1.21rc2
layout: post
---

<P>The second release candidate for version 1.1.21 of the CommonUNIX Printing System ("CUPS") is now available for download fromthe CUPS web site at:<PRE>    <A HREF="http://www.cups.org/software.html">http://www.cups.org/software.html<P>In accordance with the CUPS Configuration Management Plan,you now have until Monday, September 6th to test this releasecandidate to determine if there are any high-priority problemsand report them using the Software Trouble Report form at:<PRE>    <A HREF="http://www.cups.org/str.php">http://www.cups.org/str.php<P>Reports sent to the CUPS newsgroups or mailing lists are notautomatically entered into the trouble report database and willnot influence the final production release of 1.1.21, so it isvery important that you report any problems you identify usingthe form.<P>CUPS 1.1.21 is primarily a bug fix and performance tuningrelease and includes fixes for the IPP, LPD, parallel, serial,and USB backends, authentication and status processing issues inthe CUPS API, and various PostScript and PDF printing issues.The new release also adds support for Zebra label printers andIPP device URI options.<P>Changes in CUPS v1.1.21rc2:<UL>	
- The scheduler used a select() timeout of INT_MAX 	seconds when there was nothing to do, which doesn't 	work on IRIX (Issue #864) 	
- Updated the cupsaddsmb program to use the new Windows 	2000 PostScript drivers instead of the Windows NT 	printer drivers (Issue #390) 	
- The gziptoany filter did not produce copies for raw 	print jobs (Issue #808) 	
- The cupsLangGet() function now uses nl_langinfo(), 	when available, to get the current encoding (Issue #856) 	
- Added a ReloadTimeout directive to control how long 	the scheduler waits for jobs to complete before 	restarting the scheduler (Issue #861) 	
- Added a note to the default cupsd.conf file which 	mentions that you must allow connections from 	localhost for the command-line and web interfaces to 	work (Issue #850) 	
- The IPP backend incorrectly used the local port when 	communicating with a remote server; this caused 	problems with some custom configurations (Issue #852) 	
- The cups-lpd mini-daemon wasn't using the right 	default banner option (Issue #851) 	
- Updated the new httpDecode64_2() and httpEncode64_2() 	functions to handle arbitrary binary data, not just 	text (Issue #860) 	
- String options with quotes in their values were not 	quoted properly by the scheduler (Issue #839) 	
- Configure script changes for GNU/Hurd (Issue #838) 	
- The lppasswd program was not installed properly by GNU 	install when the installer was not root (Issue #836) 	
- Updated the cups-lpd man page (Issue #843) 	
- Fixed a typo in the cupsd man page (Issue #833) 	
- The USB backend now defaults to using the newer 	/dev/usb/lpN filenames; this helps on systems which 	use the devfs filesystem type on Linux (Issue #818) 	
- The config.h file did not define the HAVE_USERSEC_H 	constant when the configure script detected the 	usersec.h header file. This caused authentication 	errors on AIX (Issue #832) 	
- The lp and lpr commands now report the temporary 	filename and error if they are unable to create a 	temporary file (Issue #812) 	
- Added ServerTokens directive to control the Server 	header in HTTP responses (Issue #792) 	
- Added new httpDecode64_2(), httpEncode64_2(), and 	httpSeparate2() functions which offer buffer size 	arguments (Issue #797) 	
- The cupsGetFile() and cupsPutFile() code did not 	support CDSA or GNUTLS (Issue #794) 	
- The httpSeparate() function did not decode all 	character escapes (Issue #795) 	
- The cupstestppd program now checks for invalid Duplex 	option choices and fails PPD files that use 	non-standard values (Issue #791) 	
- Updated the printer name error message to indicate 	that spaces are not allowed (Issue #675) 	
- The scheduler didn't handle HTTP GET form data 	properly (Issue #744) 	
- The pstops filter now makes sure that the prolog code 	is sent before the setup code (Issue #776) 	
- The pstops filter now handles print files that 	incorrectly start @PJL commands without a language 	escape (Issue #734) 	
- Miscellaneous build fixes for NetBSD (Issue #788) 	
- Added support for quoted system group names (Issue #784) 	
- Added "version" option to IPP backend to workaround 	serious bug in Linksys's IPP implementation (Issue #767) 	
- Added Spanish translation of web interface (Issue #772, 	Issue #802) 	
- The LPD backend now uses geteuid() instead of getuid() 	when it is available (Issue #752) 	
- The IPP backend did not report the printer state if 	the wait option was set to "no" (Issue #761) 	
- The printer state was not updated for "STATE: foo,bar" 	messages (Issue #745) 	
- Added new CUPS API convenience functions which accept 	a HTTP connection to eliminate extra username/password 	prompts. This resolves a previous authentication 	caching issue (Issue #729, Issue #743) 	
- The scheduler did not correctly throttle the browse 	broadcasts, resulting in missing printers on client 	machines (Issue #754) 	
- The scheduler did not pass the correct CUPS_ENCRYPTION 	setting to CGI programs which caused problems on 	systems which used non-standard encryption settings 	(Issue #773) 	
- The lpq command showed 11st, 12nd, and 13rd instead of 	11th, 12th, and 13th for the rank (Issue #769) 	
- "make install" didn't work on some platforms due to an 	error in the man page makefiles (Issue #775) 	
- Changed some calls to snprintf() in the scheduler to 	SetStringf() (Issue #740)
