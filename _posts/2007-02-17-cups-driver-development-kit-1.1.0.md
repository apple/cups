---
title: CUPS Driver Development Kit 1.1.0
layout: post
permalink: /blog/:year-:month-:day-:title.html
---

Version 1.1.0 of the Common UNIX Printing System Driver Development Kit is now available for download from the CUPS web site at:

     http://www.cups.org/ddk/software.html

The new release adds support for creating globalized and compressed PPDs with configurable line endings, includes a new ppdmerge utility, and fixes some platform and packaging issues.
The CUPS Driver Development Kit (DDK) provides a suite of standard drivers, a PPD file compiler, and other utilities that can be used to develop printer drivers for CUPS and other printing environments.  CUPS provides a portable printing layer for UNIX&reg;-based operating systems.  The CUPS DDK provides the means for mass-producing PPD files and drivers/filters for CUPS-based printer drivers.
The CUPS DDK is licensed under the GNU General Public License.  Please contact Easy Software Products for commercial support and "binary distribution" rights.
Changes include:
- The ppdpo utility no longer includes the LanguageEncoding or LanguageVersion strings (Issue #1525) 
- The PPD compiler now provides a -D option to set variables from the command-line (Issue #2066) 
- If the PCFileName uses a lowercase ".ppd" extension, the PPD compiler will not convert the entire filename to lowercase (Issue #2065) 
- Added the ppdmerge utility. 
- The "dymo" driver has been renamed to "label", which is the name used in CUPS 1.2 and higher. 
- The PPD compiler now supports generation of compressed PPD files. 
- The PPD compiler now supports generation of PPD files with line endings other than just a line feed. 
- The PPD compiler now supports generation of globalized (multi-language) PPD files. 
- Fixed the MacOS X Universal Binary support. 
- The drivers now have their own man pages. 
- The drivers are now bundled in a separate "cupsddk-drivers" package to allow vendors to provide the drivers separate from the developer kit.
