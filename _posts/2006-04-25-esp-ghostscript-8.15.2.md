---
title: ESP Ghostscript 8.15.2
layout: post
---

ESP Ghostscript 8.15.2 is the second stable release based on GPL Ghostscript 8.15 which adds enhanced CUPS raster support for CUPS 1.2, improves the Open Printing Vector API driver, updates the CID font support files, and fixes several bugs that were reported against 8.15.1.
Changes in 8.15.2:
- Ghostscript error messages now start with CUPS prefix strings (ERROR: or CRIT:, as appropriate) 
- Updated the setpagedevice support to allow Duplex, ManualFeed, and Tumble attributes to be set, even if the underlying driver does not implement them (Issue #1598) 
- The eplaser driver incorrectly errored out if TRUE and FALSE were already defined (Issue #1336) 
- Fixed install-shared target to work on non-shared installs (Issue #1334) 
- Dynamically loaded driver support incorrectly required X11 (Issue #1351) 
- The pdfopt utility didn't guard against passing the same filename twice, which would cause the PDF file to become corrupted (Issue #1399) 
- The cdj driver incorrectly closed the device when changing BitsPerPixel values (Issue #1577) 
- Updated the CMap files to the latest (Issue #1345) 
- PostScript files that set the ManualFeed attribute didn't work (Issue #1570) 
- Now unconditionally include <time.h> to work around a bug in the Compaq C compiler header files (Issue #1539, Issue #1549) 
- gv did not work with the gsx of ESP GhostScript with shared libgs (Issue #1419, Issue #1433) 
- Added support for CUPS 1.2 cupsBorderlessScalingFactor, cupsImagingBBox, cupsPageSize, and cupsPageSizeName page device attributes (Issue #1406) 
- Updated Epson-Avasys driver for the Epson laser printers to the newest version. Added devices "lps4500" and "lps6500" to support the newest models (Issue #1507, thanks to Olaf Meeuwissen from Avasys). 
- The opvp driver fixed the bug that 1bpp bitmap was printed in reverse color. 
- The opvp driver do not ignore blank page 
- The opvp drievr use snprintf instead of sprintf to  avoid a few potential security holes. 
- The opvp driver use fabs function instead of fabsf function (Issue #1291). 
- Allow non-standard glyph names when synthesising an Encoding  for 'glyphshow' to avoid the non-standard TTF glyphs being expanded to outlines (Issue #1455). 
- Updated KRGB support in the "ijs" device to version 1.2, fixing several buffer overflows and memory leaks, especially avoiding segfaults when printing full-bleed with the HPIJS driver on HP inkjets (Thanks to David Suffield from HP). 
- Fixed rendering of images when converting PostScript to PDF with "ps2pdf", fixed also a crash when generating PDF files with the "pdfwrite" device (Thanks to Werner Fink from SuSE). 
- Some files of the shared X11 driver were still not built with CC_SHARED (gdevxcmp.c, gsparamx.c). 
- libijs had still some hard-coded /usr/lib, this broke building on 64-bit systems. 
- Build the shared library of libijs as versioned library by default. 
- Fixed a SEGV. It seems that the new vector device makes the bbox device doing an allocation in gx_general_fill_path(). Seems to have fixed Issue #1116 (Thanks to Werner Fink from SuSE). 
- Applied fix for vertical japanese text from http://www.gssm.otsuka.tsukuba.ac.jp/staff/ohki/gs850-patch-mine (Thanks to Werner Fink from SuSE). 
- Adapted the color model in the "pcl3" driver to GhostScript 8.15 (Thanks to Werner Fink from SuSE). 
- Fixed a memory overflow in the "lips4" driver (Thanks to Werner Fink from SuSE). 
- "cgm*" drivers are now able to write onto a pipe (Thanks to Werner Fink from SuSE). 
- Double free fixed (in gsdevice.c, Thanks to Werner Fink from SuSE). 
- SEGV in "inferno" driver because the struct "inferno_device" was not created but it was accessed to its elements (Thanks to Werner Fink from SuSE). 
- Adapted the color model in the "devicen" driver to GhostScript 8.15 (Thanks to Werner Fink from SuSE). 
- Wrongly entered paper dimensions in the "cljet5" driver fixed (Thanks to Werner Fink from SuSE).
