---
title: Common UNIX Printing System 1.3.2
layout: post
---

CUPS 1.3.2 is now available for download from the CUPS web site. CUPS 1.3.2 replaces the invalid 1.3.1 release tarballs and fixes some scheduler and printing issues. Changes include:	
- The 1.3.1 release was incorrectly created from the 1.4.x source tree (Issue #2519)	
- Added support for 32/64-bit libraries on HP-UX (Issue #2520)	
- The scheduler incorrectly used portrait as the default orientation (Issue #2513)	
- The scheduler no longer writes the printcap file for every remote printer update (Issue #2512)	
- Remote raw printing with multiple copies did not work (Issue #2518)	
- Updated the configure script to require at least autoconf 2.60 (Issue #2515)	
- Some gzip'd PPD files were not read in their entirety (Issue #2510)
