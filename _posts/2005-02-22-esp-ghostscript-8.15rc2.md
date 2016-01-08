---
title: ESP Ghostscript 8.15rc2
layout: post
---

ESP Ghostscript 8.15rc2 is the second release candidate based on GPL Ghostscript 8.15 and includes an enhanced configure script, the CUPS raster driver, many GPL drivers, support for dynamically loaded drivers (currently implemented for the X11 driver), and several GPL Ghostscript bug fixes. The new release also fixes all of the reported STRs from ESP Ghostscript 7.07.x.

In accordance with the CUPS Configuration Management Plan, you now have until Tuesday, March 8th to test this release candidate to determine if there are any high-priority problems and report them using the Software Trouble Report form at:

    http://www.cups.org/espgs/str.php

Reports sent to the CUPS newsgroups or mailing lists are not automatically entered into the trouble report database and will not influence the final production release of ESP Ghostscript, so it is very important that you report any problems you identify using the form.

Changes in 8.15rc2:

- The PCL XL driver now supports duplexing and media sources.
- The CUPS driver now supports choosing a media size by dimensions and bottom-left margins (Issue #855)
- Fixed IJS driver bug in KRGB and 1-bit colorspace support (Issue #1077)
- Added many old japanese printer drivers: "ljet4pjl", "lj4dithp", "dj505j", "picty180", "lips2p", "bjc880j", "pr201", "pr150", "pr1000", "pr1000_4", "jj100", "bj10v", "bj10vh", "mag16", "mag256", "mj700v2c", "mj500c", "mj6000c", "mj8000c", "fmpr", "fmlbp", "ml600", "lbp310", "lbp320", "md50Mono", "md50Eco", "md1xMono"
- Fixed PCL-XL driver color bug (Issue #1080)
- Merged bug fixes from GPL Ghostscript CVS (Issue #1083)
- Fixed segfault problem on PowerPC platform (Issue #1079)
- Fixed building of dynamically linked X drivers ("./configure --enable-dynamic")
- Fixed problem with compiling both of the LIPS drivers at the same time (Issue #1078)
- Added missing opvp and oprp drivers to configure script.

