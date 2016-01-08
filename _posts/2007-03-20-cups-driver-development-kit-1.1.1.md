---
title: CUPS Driver Development Kit 1.1.1
layout: post
---

Version 1.1.1 of the Common UNIX Printing System Driver Development Kit is now available for download from the CUPS web site at:

     http://www.cups.org/ddk/software.html

The new release fixes a bug in ppdmerge when importing Japanese PPD files.
The CUPS Driver Development Kit (DDK) provides a suite of standard drivers, a PPD file compiler, and other utilities that can be used to develop printer drivers for CUPS and other printing environments.  CUPS provides a portable printing layer for UNIX&reg;-based operating systems.  The CUPS DDK provides the means for mass-producing PPD files and drivers/filters for CUPS-based printer drivers.
The CUPS DDK is licensed under the GNU General Public License.  Please contact Easy Software Products for commercial support and "binary distribution" rights.
Changes include:
- The ppdmerge utility incorrectly used "jp" for the Japanese locale (Issue #2300)
