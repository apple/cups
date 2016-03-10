---
title: ipptool January 2013 Release
layout: post
permalink: /blog/:year-:month-:day-:title.html
---

The January 2013 release of ipptool for Windows, Linux (32-bit and 64-bit Intel), and Mac OS X is now available at:

    http://www.cups.org/software.html

This release of ipptool adds several new features and a preliminary IPP Everywhere test file.  The IPP Everywhere test file uses the sample PWG Raster files published by the Printer Working Group, which due to their size are available separately at the following location:

    ftp://ftp.pwg.org/pub/pwg/ipp/examples/

Changes include:

- Added support for DEFINE-MATCH and DEFINE-NO-MATCH as STATUS predicates.
- Added support for WITH-VALUE and resolution values.
- Added support for SKIP-IF-MISSING (skip test if file is missing)
- Added support for octetString values.
- Added support for document compression in Print-Job and Send-Document requests.
- Fixed REPEAT-MATCH for STATUS and EXPECT - was incorrectly erroring out.
- Fixed a bug where bad IPP responses would cause ipptool to crash.

Enjoy!

