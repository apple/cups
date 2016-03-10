---
title: CUPS Driver for Windows v6.0 Nearing Release
layout: post
permalink: /blog/:year-:month-:day-:title.html
---

We have made major progress on the new CUPS driver for Windows, and will be doing a release candidate very soon. The new driver adds support for the page-label and job-billing options that are missing from the standard Windows PostScript driver.

The new driver adds three files to the Microsoft PostScript driver: <var>cups6.ini</var>, <var>cupsps6.dll</var>, and <var>cupsui6.dll</var>. The .DLL files are available both as precompiled binaries and as source code provided under the GNU General Public License. You can access the current development version of the driver at:

    http://svn.easysw.com/public/windows/trunk/

You will need the Microsoft Windows Driver Development Kit and Visual C++ 6.0 or higher to compile the driver source code. The <tt>cupsaddsmb</tt> program must be updated to use the new driver - source code for the updated program is available in both the CUPS and Windows driver repositories.

The new driver works on Microsoft Windows 2000, XP, and 2003. We have no plans to support Windows NT, Windows 9x, or Windows ME.

