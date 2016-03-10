---
title: includefonts 0.10
layout: post
permalink: /blog/:year-:month-:day-:title.html
---

Includefonts is a filter for the CUPS print system, which embeds the fonts required by PostScript print jobs, given that the PostScript contains the necessary DSC comments and that the required fonts are available on the system.
Notable changes since the first release are:
- Includefonts now comes with its own CUPS interface.  This removes the dependency on the Net::CUPS module, which makes includefonts easier to build.
- Includefonts has always had the capability to download TrueType fonts as Type 42 fonts.  However, TrueType fonts had to be added manually to PSres.upr since makepsres doesn't recognize them.  Includefonts now comes with a small utility (ttfupr) for building .upr files for TrueType fonts.
- Includefonts and pslines are now installed into the standard Perl scripts directory and linked into the CUPS filter directory.  Because everything is installed in standard locations, this should make administration easier.
- Expanded and updated documentation
