---
title: ESP Ghostscript 8.15rc4
layout: post
permalink: /blog/:year-:month-:day-:title.html
---

ESP Ghostscript 8.15rc4 is the fourth release candidate based on
GPL Ghostscript 8.15 and includes an enhanced configure script,
the CUPS raster driver, many GPL drivers, support for dynamically
loaded drivers (currently implemented for the X11 driver), and
several GPL Ghostscript bug fixes. The new release also fixes all
of the reported STRs from ESP Ghostscript 7.07.x.

In accordance with the CUPS Configuration Management Plan, you
now have until Thursday, August 25th to test this release
candidate to determine if there are any high-priority problems
and report them using the Software Trouble Report form at:

    http://www.cups.org/espgs/str.php

Reports sent to the CUPS newsgroups or mailing lists are not
automatically entered into the trouble report database and will
not influence the final production release of ESP Ghostscript, so
it is very important that you report any problems you identify
using the form.

Changes in 8.15rc4:

- Merged fixes from the GPL Ghostscript repository.
- Fixed multiple media selection bugs (Issue #1172, Issue #1204)
- Fix a FreeType bug on 64-bit platforms (Issue #1235)
- Fixed problems when rendering certain PostScript files on 64-bit platforms (Issue #1168, thanks to Werner Fink from SuSE)
- Added long standing update from the author of "bjc600" and "bjc800" (Thanks to Werner Fink from SuSE)
- Switched back to old color model in the "bjc600" and "bjc800" drivers. Now one can use "-dBitsPerPixel=1" without getting an error message and a segfault (Thanks to Werner Fink from SuSE)
- Use dci macro in the "lx5000" driver (Thanks to Werner Fink from SuSE)
- Resource directory now determined based on CMap directory, without this change the first directory from LIBPATH is taken, and so, if set, GS_LIB selects the directory and this usually leads to an error (Thanks to Werner Fink from SuSE)
- Cleaned up the source distribution, removing non-free files from the original GPL Ghostscript release (Issue #1165)
- Fixed an obscure CMYK rendering bug (Issue #1152)
- Optimized 16-bit output from the CUPS raster device.
- Correct the inappropriate pitch bytes handling for drawing bitmaps on 64bit.

