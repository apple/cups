---
title: Common UNIX Printing System 1.2.11
layout: post
permalink: /blog/:year-:month-:day-:title.html
---

<P>CUPS 1.2.11 is now available for download from the CUPS web site at:</P><PRE>    <A HREF="http://www.cups.org/software.html">http://www.cups.org/software.html</A></PRE><P>CUPS 1.2.11 fixes several build system, printing, PPD, and IPP conformance issues. It also fixes a crash bug in the scheduler when printing to files in non-existent directories. Changes include:</P>
- Updated the launchd support on Mac OS X to better support reconfiguration. 
- "make distclean" didn't remove all generated files (Issue #2366) 
- Fixed a bug in the advertisement of classes (Issue #2373) 
- The IPP backend now stays running until the job is actually printed by the remote server; previously it would stop monitoring the job if it was held or temporarily stopped (Issue #2352) 
- PDF files were not always printed using the correct orientation (Issue #2348) 
- The scheduler could crash if you specified a bad file: URI for a printer (Issue #2351) 
- The Renew-Subscription operation now returns the notify-lease-duration value that was used (Issue #2346) 
- The IPP backend sent job options to IPP printers, however some printers tried to override the options embedded in the PS/PCL stream with those job options (Issue #2349) 
- ppdLocalize() now also tries a country-specific localization for when localizing to a generic locale name. 
- The cupstestppd program now allows for partial localizations to reduce the size of universal PPD files. 
- Chinese PPD files were incorrectly tagged with the "cn" locale (should have been "zh") 
- The backends now manage the printer-state-reasons attribute more accurately (Issue #2345) 
- Java, PHP, Perl, and Python scripts did not work properly (Issue #2342) 
- The scheduler would take forever to start if the maximum number of file descriptors was set to "unlimited" (Issue #2329) 
- The page-ranges option was incorrectly applied to the banner pages (Issue #2336) 
- Fixed some GCC compile warnings (Issue #2340) 
- The DBUS notification code was broken for older versions of DBUS (Issue #2327) 
- The IPv6 code did not compile on HP-UX 11.23 (Issue #2331) 
- PPD constraints did not work properly with custom options. 
- Regular PPD options with the name "CustomFoo" did not work. 
- The USB backend did not work on NetBSD (Issue #2324) 
- The printer-state-reasons attribute was incorrectly cleared after a job completed (Issue #2323) 
- The scheduler did not set the printer operation policy on startup, only on soft reload (Issue #2319) 
- The AP_FIRSTPAGE_InputSlot option did not clear any ManualFeed setting that was made, which caused problems with some PPD files (Issue #2318) 
- cupsDoFileRequest() and cupsDoRequest() did not abort when getting an error in the response (Issue #2315) 
- The scheduler did not schedule jobs properly to remote or nested classes (Issue #2317) 
- Updated the mime.types and mime.convs headers to warn that the files are overwritten when CUPS is installed. Local changes should go in local.types or local.convs, respectively (Issue #2310) 
- The scheduler could get in an infinite loop if a printer in an implicit class disappeared (Issue #2311) 
- The pstops filter did not handle %%EndFeature comments properly (Issue #2306) 
- Fixed a problem with the Polish web page printer icons (Issue #2305) 
- ppdLocalize() now also localizes the cupsICCProfile attributes. 
- The scheduler still had a reference to the incorrect "notify-recipient" attribute (Issue #2307) 
- The "make check" and "make test" subscription tests did not set the locale (Issue #2307) 
- The "make check" and "make test" subscription tests incorrectly used the notify-recipient attribute instead of notify-recipient-uri (Issue #2307) 
- cupsRasterInterpretPPD() incorrectly limited the cupsBorderlessScalingFactor when specified in the job options.
