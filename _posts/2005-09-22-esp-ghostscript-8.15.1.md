---
title: ESP Ghostscript 8.15.1
layout: post
permalink: /blog/:year-:month-:day-:title.html
---

ESP Ghostscript 8.15.1 is the first stable release based on
GPL Ghostscript 8.15 and includes an enhanced configure script,
the CUPS raster driver, many GPL drivers, support for dynamically
loaded drivers (currently implemented for the X11 driver), and
several GPL Ghostscript bug fixes. The new release also fixes all
of the reported STRs from ESP Ghostscript 7.07.x.

Changes in 8.15.1:

- The shared X11 driver was not built with the correct linker command (CCLD instead of CC_SHARED) (Issue #1255)
- The opvp driver incorrectly assumed that CODESET was supported on all platforms that supported iconv (Issue #1247)
- Updated the iconv checks so they work on more platforms (Issue #1154)
- Added support in the "cups" driver for the CUPS_CSPACE_RGBW colorspace (new in MacOS X 10.4 and CUPS 1.2)
- Added "SET RENDERMODE=..." PJL command to header of the output of the "pxlmono" and "pxlcolor" drivers, this way color laser printers get correctly switched between grayscale and color mode and are this way often four times faster in grayscale (Thanks to Jonathan Kamens, jik at kamens dot brookline dot ma dot us, for this fix).
- Corrected Legal paper size definition for the "pxlmono" and  "pxlcolor" drivers (Thanks to Jonathan Kamens, jik at kamens  dot brookline dot ma dot us, for this fix).
- Added some missing "$(install_prefix)"in the install procedure of the extra files for "pcl3" driver.
- The lips4 driver used a small string buffer to hold the output resolution, which would cause a buffer overflow for resolutions > 99 (Issue #1241).

