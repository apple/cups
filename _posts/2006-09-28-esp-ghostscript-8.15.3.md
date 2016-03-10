---
title: ESP Ghostscript 8.15.3
layout: post
permalink: /blog/:year-:month-:day-:title.html
---

ESP Ghostscript 8.15.3 is the third stable release based on GPL Ghostscript 8.15 which fixes CUPS driver, CJKV font support, IJS KRGB support, various compile problems, and several small issues in the command-line utilities.
Changes in ESP Ghostscript 8.15.3:
- The install_prefix variable was not used consistently when installing from source (Issue #1949) 
- PageSize policy 3 was unimplementable and has been converted to policy 7, impose size (Issue #1794) 
- Duplex, Tumble, and ManualFeed were not working properly (Issue #1987) 
- pdf2ps could generate a "null setpagesize" command (Issue #1641) 
- CJK font handling fix (Issue #1639) 
- Fixed a crash bug with the X11 driver (Issue #1635) 
- Added support for GTK+ 2.0 (Issue #1633) 
- Added dynamically loaded driver support for *BSD (Issue #1628) 
- "make install" didn't work without --enable-dynamic on some platforms (Issue #1611) 
- Fixed a pdf2ps error with images (Issue #1779) 
- Translated the Japanese comments in addons/opvp/opvp_common.h (OpenPrinting Vector driver, device "opvp") to english (STR  #1844, thanks to Todd Fujinaka from Intel). 
- Updated KRGB support in the "ijs" device to version 1.3, fixing bugs and adding KRGB 1-bit and 8-bit support (Thanks to David Suffield from HP). 
- CJKV support will be available as default. 
- Added gs8 CJKV patch 
- Fix compilation on systems that don't have gtk (but still want the x11 driver) 
- The omni driver doesn't use glib -- update configure.ac accordingly 
- Fix ps2epsi in locales where ~ comes before ! (Issue #1643) 
- Use mktemp in ps2epsi if available (Issue #1630) 
- New --enable/disable-fontconfig switch, allows the use of fontconfig to retreive fonts lists on Unix (Issue #1631, based on patch from Craig Ritter) 
- Make fapi_ft compile with system freetype (Issue #1632)
