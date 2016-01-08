---
title: ESP Ghostscript 8.15rc3
layout: post
---

ESP Ghostscript 8.15rc3 is the third release candidate based on GPL Ghostscript 8.15 and includes an enhanced configure script, the CUPS raster driver, many GPL drivers, support for dynamically loaded drivers (currently implemented for the X11 driver), and several GPL Ghostscript bug fixes. The new release also fixes all of the reported STRs from ESP Ghostscript 7.07.x.

In accordance with the CUPS Configuration Management Plan, you now have until Thursday, May 4th to test this release candidate to determine if there are any high-priority problems and report them using the Software Trouble Report form at:

    http://www.cups.org/espgs/str.php

Reports sent to the CUPS newsgroups or mailing lists are not automatically entered into the trouble report database and will not influence the final production release of ESP Ghostscript, so it is very important that you report any problems you identify using the form.

Changes in 8.15rc3:

- The ESP Ghostscript utility scripts (ps2ps, ps2pdf, etc.) now try to run the "gs" command in the same directory as the script first.  This allows multiple versions of ESP Ghostscript to coexist more easily (Issue #1125)
- The pswrite device made its dictionary readonly, which caused problems with certain PostScript printers (Issue #1100)
- ps2epsi failed when invoking sed (Issue #261)
- The CUPS device now supports 16-bit per color rendering.
- Fix of the buffer alignment of the drivers "imagen" and "lx5000" (Thanks to Werner Fink from SuSE).
- Fix to get the TTF engine to work even on 64bit architectures (Thanks to Werner Fink from SuSE).
- Fix to allow a printer driver to switch the polarity in the case  of switching from gray mode into color mode, e.g. with  "-dBitsPerPixel=1" (Thanks to Werner Fink from SuSE).
- Enabled the usage of pipes within -sOutputFile even for the old  japanese printers, "alc1900", and others (Thanks to Werner Fink from SuSE).
- Corrected the PPD files cbjc600.ppd, cbjc800.ppd, ghostpdf.ppd to be fully Adobe-compliant so that they are accepted by CUPS (Thanks to Werner Fink from SuSE).
- Antialiasing fix for X screen display (Thanks to Werner Fink from SuSE).
- 64-bit pointer align fix (Thanks to Werner Fink from SuSE).
- Fix to copy all image information (Thanks to Werner Fink from SuSE).
- Fix the opvp problem that fails to handle index colored BW images.
- The PNG devices were missing from the configure script (Issue #1113)
- The opvp driver requires both dlopen() and iconv_open() support, which were not checked in the configure script (Issue #1107)
- RPMs created using the ghostscript.spec file did not include the correct fontpath (Issue #1112)
- The previous default mapping of stdout to stderr caused problems for some programs (Issue #1108)
- Re-added section to README which tells that all non-obsolete compile-in drivers and UPP files as listed on linuxprinting.org are included
- Added remaining old Japanese printer drivers: "dmprt",  "escpage", "lp2000", "npdl", "rpdl" (Thanks to Werner Fink from SuSE for converting them to the new API, Issue #1094)
- Fixed C99-isms in several add-on drivers that prevented compiles to work on non-C99 compilers (Issue #1104, Issue #1114)
- Fixed segfault problem of opvp driver on AMD64.
- Fixed gamma correction bug of opvp driver.
- Fixed illegal color conversion bug on opvp gray device.
- Fixed opvp segfault problem caused by Ghostscript 8.15 font rasterization.
- Fixed lips4v segfault problem caused by Ghostscript 8.15 font rasterization.
- Fixed Makefile.in to also compile the PNG and JBIG2 stuff
- Added the free Epson Kowa laser printer drivers for the EPL (non-L) series, the AcuLaser series, and the japanese LP series

