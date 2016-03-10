---
title: CUPS Driver Development Kit 1.2.0
layout: post
permalink: /blog/:year-:month-:day-:title.html
---

Version 1.2.0 of the Common UNIX Printing System Driver Development Kit is now available for download from the CUPS web site at:

     http://www.cups.org/ddk/software.html

The new release fixes several localization issues and adds support for many more languages.
The CUPS Driver Development Kit (DDK) provides a suite of standard drivers, a PPD file compiler, and other utilities that can be used to develop printer drivers for CUPS and other printing environments.  CUPS provides a portable printing layer for UNIX&reg;-based operating systems.  The CUPS DDK provides the means for mass-producing PPD files and drivers/filters for CUPS-based printer drivers.
The CUPS DDK is licensed under the GNU General Public License version 2.
Changes include:
- The DDK is now owned and licensed by Apple Inc. 
- Added many new and updated message catalogs for the default localization strings. 
- The ppdc utility did not generate localized PageSize, InputSlot, or MediaType options. 
- The ppdpo utility incorrectly included the copyright text in the .po file. 
- The ppdc utility incorrectly included two copies of the  file. 
- The ppdc utility did not allow you to override the cupsVersion attribute. 
- The ppdmerge utility now recognizes "Korean" as a LanguageVersion. 
- The ppdmerge utility incorrectly used "cn" for the Chinese locale.
