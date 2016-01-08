---
title: Common UNIX Printing System 1.4b2
layout: post
---

CUPS 1.4b2 is now available for download from:

    http://www.cups.org/software.html

The second beta release of CUPS 1.4 fixes several localization, scheduler, and utility issues, improves the performance of several key CUPS APIs, and adds a Spanish localization. Changes include:

- Documentation updates (Issue #2983, Issue #2998, Issue #3021)
- The cupstestppd utility now validates the FileVersion and
FormatVersion values in PPD files.
- The default cupsd.conf file did not reflect the
--with-local-protocols value set at compile-time (Issue #3037)
- The cupsGetPPD* APIs now create symlinks to local PPD files
rather than copying them whenever possible.
- Various performance optimizations in the string pool, dests, and
options implementations.
- The cupsGetDests* APIs now return the marker and printer-commands
attributes.
- Side-channel SNMP lookups would not work when cupsSNMPSupplies
was set to False in the PPD file.
- Localized the device descriptions for the SCSI, serial,
and network backends (Issue #3014)
- Added a Spanish localization (Issue #3015)
- Added support for marker-low-levels and marker-high-levels
attributes.
- The scheduler could hang writing a long log line.
- The cupsGetDevices() function now has an "include_schemes"
parameter.
- The lpinfo command now supports --include-schemes and
--exclude-schemes options.
- The CUPS-Get-PPDs operation now supports the include-schemes
and exclude-schemes attributes.
- The CUPS-Get-Devices operation now supports the include-schemes
attribute.
- The print filters now support a replacement for the fitplot
option called "fit-to-page".
- The LPD backend no longer tries to collect page accounting
information since the LPD protocol does not allow us to
prevent race conditions.
- The scheduler did not save the last marker-change-time value.
- Fixed a problem with printing to some IPP printers, including
CUPS 1.1.x.
- Fixed a redirection problem with the printer web page (Issue #3012)
- Fixed a PPD compiler problem with the loading of message
catalogs (Issue #2990)
- Fixed a PPD compiler problem with the loading of .strings files
(Issue #2989)
- The cupsfilter utility did not set the CONTENT_TYPE environment
variable when running filters.
- The scheduler now waits to allow system sleep until the jobs
have all stopped.
- The IPP, LPD, and socket backends used different "connecting"
progress messages.

