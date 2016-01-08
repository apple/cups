---
title: Common UNIX Printing System 1.3.8
layout: post
---

CUPS 1.3.8 is now available for download from the CUPS web site:    http://www.cups.org/software.htmlThe new release fixes some performance and printing bugs. Changes include:
- Documentation updates (Issue #2785, Issue #2861, Issue #2862)
- The scheduler did not add the ending job sheet when the job was released.
- The IPP backend did not relay marker-* attributes.
- The CUPS GNOME/KDE menu item was not localized for Chinese (Issue #2880)
- The CUPS GNOME/KDE menu item was not localized for Japanese (Issue #2876)
- The cupstestppd utility reported mixed line endings for Mac OS and Windows PPD files (Issue #2874)
- The pdftops filter did not print landscape orientation PDF pages correctly on all printers (Issue #2850)
- The scheduler did not handle expiring of implicit classes or their members properly, leading to a configuration where one of the members would have a short name (Issue #2766)
- The scheduler and cupstestppd utilities did not support cupsFilter and cupsPreFilter programs with spaces in their names (Issue #2866)
- Removed unused variables and assignments found by the LLVM &quot;clang&quot; tool.
- Added NULL checks recommended by the LLVM &quot;clang&quot; tool.
- The scheduler would crash if you started a printer that pointed to a backend that did not exist (Issue #2865)
- The ppdLocalize functions incorrectly mapped all generic locales to country-specific locales.
- The cups-driverd program did not support Simplified Chinese or Traditional Chinese language version strings (Issue #2851)
- Added an Indonesian translation (Issue #2792)
- Fixed a timing issue in the backends that could cause data corruption with the CUPS_SC_CMD_DRAIN_OUTPUT side-channel command (Issue #2858)
- The scheduler did not support &quot;HostNameLookups&quot; with all of the boolean names (Issue #2861)
- Fixed a compile problem with glibc 2.8 (Issue #2860)
- The PostScript filter did not support %%IncludeFeature lines in the page setup section of each page (Issue #2831)
- The scheduler did not generate printer-state events when the default printer was changed (Issue #2764)
- cupstestppd incorrectly reported a warning about the PPD format version in some locales (Issue #2854)
- cupsGetPPD() and friends incorrectly returned a PPD file for a class with no printers.
- The member-uris values for local printers in a class returned by the scheduler did not reflect the connected hostname or port.
- The CUPS PHP extension was not thread-safe (Issue #2828)
- The scheduler incorrectly added the document-format-default attribute to the list of &quot;common&quot; printer attributes, which over time would slow down the printing system (Issue #2755, Issue #2836)
- The cups-deviced and cups-driverd helper programs did not set the CFProcessPath environment variable on Mac OS X (Issue #2837)
- &quot;lpstat -p&quot; could report the wrong job as printing (Issue #2845)
- The scheduler would crash when some cupsd.conf directives were missing values (Issue #2849)
- The web interface &quot;move jobs&quot; operation redirected users to the wrong URL (Issue #2815)
- The Polish web interface translation contained errors (Issue #2815)
- The scheduler did not report PostScript printer PPDs with filters as PostScript devices.
- The scheduler did not set the job document-format attribute for jobs submitted using Create-Job and Send-Document.
- cupsFileTell() did not work for log files opened in append mode (Issue #2810)
- The scheduler did not set QUERY_STRING all of the time for CGI scripts (Issue #2781, Issue #2816)
- The scheduler now returns an error for bad job-sheets values (Issue #2775)
- Authenticated remote printing did not work over domain sockets (Issue #2750)
- The scheduler incorrectly logged errors for print filters when a job was canceled (Issue #2806, #2808)
- The scheduler no longer allows multiple RSS subscriptions with the same URI (Issue #2789)
- The scheduler now supports Kerberized printing with multiple server names (Issue #2783)
- &quot;Satisfy any&quot; did not work in IPP policies (Issue #2782)
- The CUPS imaging library would crash with very large images - more than 16Mx16M pixels (Issue #2805)
- The PNG image loading code would crash with large images (Issue #2790)
- The scheduler did not limit the total number of filters.
- The scheduler now ensures that the RSS directory has the correct permissions.
- The RSS notifier did not quote the feed URL in the RSS file it created (Issue #2801)
- The web interface allowed the creation and cancellation of RSS subscriptions without a username (Issue #2774)
- Increased the default MaxCopies value on Mac OS X to 9999 to match the limit imposed by the print dialog.
- The scheduler did not reject requests with an empty Content-Length field (Issue #2787)
- The scheduler did not log the current date and time and did not escape special characters in request URIs when logging bad requests to the access_log file (Issue #2788)
