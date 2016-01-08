---
title: Common UNIX Printing System 1.3.7
layout: post
---

CUPS 1.3.7 is now available for download from the CUPS web site:    http://www.cups.org/software.htmlThe new release includes three security fixes and several printing and authentication fixes. We encourage all CUPS users to update to the current release.Changes include:
- CVE-2008-0047: cgiCompileSearch buffer overflow (Issue #2729)
- CVE-2008-1373: CUPS GIF image filter overflow (Issue #2765)
- Updated the &quot;make check&quot; tests to do a more thorough automated test.
- cups-driverd complained about missing directories (Issue #2777)
- cupsaddsmb would leave the Samba username and password on disk if no Windows drivers were installed (Issue #2779)
- The Linux USB backend used 100% CPU when a printer was disconnected (Issue #2769)
- The sample raster drivers did not properly handle SIGTERM (Issue #2770)
- The scheduler sent notify_post() messages too often on Mac OS X.
- Kerberos access to the web interface did not work (Issue #2748)
- The scheduler did not support &quot;AuthType Default&quot; in IPP policies (Issue #2749)
- The scheduler did not support the &quot;HideImplicitMembers&quot; directive as documented (Issue #2760)
- &quot;make check&quot; didn't return a non-zero exit code on error (Issue #2758)
- The scheduler incorrectly logged AUTH_foo environment variables in debug mode (Issue #2751)
- The image filters inverted PBM files (Issue #2746)
- cupsctl would crash if the scheduler was not running (Issue #2741)
- The scheduler could crash when printing using a port monitor (Issue #2742)
- The scheduler would crash if PAM was broken (Issue #2734)
- The image filters did not work with some CMYK JPEG files produced by Adobe applications (Issue #2727)
- The Mac OS X USB backend did not work with printers that did not report a make or model.
- The job-sheets option was not encoded properly (Issue #2715)
- The scheduler incorrectly complained about missing LSB PPD directories.
