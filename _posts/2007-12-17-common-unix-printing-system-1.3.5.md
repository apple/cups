---
title: Common UNIX Printing System 1.3.5
layout: post
---

CUPS 1.3.5 is now available from the CUPS web site and fixes some SNMP and PDF filter security issues, some USB printing issues, and several scheduler issues. Changes include:
- The SNMP backend did not check for negative string lengths (Issue #2589)
- The scheduler incorrectly removed auth-info attributes, potentially leading to a loss of all options for a job.
- The scheduler stopped sending CUPS browse packets on a restart when using fixed addresses (Issue #2618)
- Fixed PDF filter security issues (CVE-2007-4352 CVE-2007-5392 CVE-2007-5393)
- Changing settings would always change the DefaultAuthType and Allow lines (Issue #2580)
- The scheduler would crash when submitting an undefined format file from Samba with LogLevel debug2 (Issue #2600)
- The scheduler did not use poll() when epoll() was not supported by the running kernel (Issue #2582)
- Fixed a compile problem with Heimdal Kerberos (Issue #2592)
- The USB backend now retries connections to a printer indefinitely rather than stopping the queue.
- Printers with untranslated JCL options were not exported to Samba correctly (Issue #2570)
- The USB backend did not work with some Minolta USB printers (Issue #2604)
- The strcasecmp() emulation code did not compile (Issue #2612)
- The scheduler would crash if a job was sent to an empty class (Issue #2605)
- The lpc command did not work in non-UTF-8 locales (Issue #2595)
- Subscriptions for printer-stopped events also received other state changes (Issue #2572)
- cupstestppd incorrectly reported translation errors for the &quot;en&quot; locale.
- ppdOpen() did not handle custom options properly when the Custom attribute appeared before the OpenUI for that option.
- The scheduler could crash when deleting a printer or listing old jobs.
- The Mac OS X USB backend did not allow for requeuing of jobs submitted to a class.
- lpmove didn't accept a job ID by itself.
- The scheduler incorrectly removed job history information for remote print jobs.
- The scheduler incorrectly sent the &quot;com.apple.printerListChanged&quot; message for printer state changes.
- The PostScript filter drew the page borders (when enabled) outside the imageable area.
- The LPD and IPP backends did not default to the correct port numbers when using alternate scheme names.
- The scheduler incorrectly deleted hardwired remote printers on system sleep.
- The scheduler would abort if a bad browse protocol name was listed in the cupsd.conf file.
- The online cupsd.conf help file incorrectly showed &quot;dns-sd&quot; instead of &quot;dnssd&quot; for Bonjour sharing.
- The scheduler could crash changing the port-monitor value.
- The scheduler generated CoreFoundation errors when run as a background process.
- When printing with number-up &gt; 1, it was possible to get an extra blank page.
