---
title: Common UNIX Printing System 1.3.1
layout: post
permalink: /blog/:year-:month-:day-:title.html
---

" This release has been pulled since the tarballs were actually snapshots of CUPS 1.4.x and not the CUPS 1.3.x branch. A 1.3.2 release is forthcoming that fixes this issue. We apologize for any inconvenience this may have caused...
CUPS 1.3.1 is now available for download from www.cups.org and fixes some build, localization, binary PostScript, and Kerberos issues. Fixes include:
- Documentation updates.
- The USB backend on Mac OS X could hang if the driver and printer did not match.
- Delegated Kerberos credentials were not working.
- &quot;make distclean&quot; incorrectly removed the edit-config.tmpl files (Issue #2508)
- Fix compile problem on HP-UX (Issue #2501)
- The cupstestppd utility now tests for resolutions greater than 99999 DPI to detect a missing  &quot;x&quot; between the X and Y resolutions.
- Fixed many problems in the various translations and added a new &quot;checkpo&quot; utility to validate them.
- The cupstestppd utility now tests the custom page size code for CUPS raster drivers.
- cupsLangDefault() did not attempt to return a language that was supported by the calling application.
- If a remote printer stopped while a job was being sent, the local queue would also get stopped and the job re-queued, resulting in duplicate prints in some cases.
- A few Apple-specific job options needed to be omitted when printing a banner page.
- The new peer credential support did not compile on FreeBSD (Issue #2495)
- Direct links to help files did not set the current section so the table-of-contents was not shown.
- The configure script did not support --localedir=foo (Issue #2488)
- The backends were not displaying their localized messages.
- CUPS-Authenticate-Job did not require Kerberos authentication on queues protected by Kerberos.
- The Zebra ZPL driver did not work with Brady label printers (Issue #2487)
- Norwegian wasn't localized on Mac OS X.
- getnameinfo() returns an error on some systems when DNS is not available, leading to numerous problems (Issue #2486)
- The cupsfilter command did not work properly on Mac OS X.
- The scheduler makefile contained a typo (Issue #2483)
- The TBCP and BCP port monitors did not handle the trailing CTRL-D in some PostScript output properly.
- Fixed the localization instructions and German template for the &quot;Find New Printers&quot; button (Issue #2478)
- The web interface did not work with the Chinese localization (Issue #2477)
- The web interface home page did not work for languages that were only partially localized (Issue #2472)
- Updated the Spanish web interface localization (Issue #2473)
- ppdLocalize() did not work for country-specific localizations.
