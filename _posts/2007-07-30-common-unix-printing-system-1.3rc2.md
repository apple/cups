---
title: Common UNIX Printing System 1.3rc2
layout: post
permalink: /blog/:year-:month-:day-:title.html
---

The second release candidate for CUPS 1.3 is now available for download from:

     http://www.cups.org/software.html

As per the CUPS Configuration Management Plan, we now start our two week "soak" of each release candidate. Once we are happy with the quality, we'll do the first stable release, 1.3.0. If you experience problems with the release candidate, please post your issues to the cups.general forum or mailing list. Confirmed bug reports should be posted to the Bugs &amp; Features page. 
CUPS 1.3 adds Kerberos and mDNS (Bonjour) support along with over 30 new features. Changes in 1.3rc2 include:
- Added more range checking to the pdftops filter.
- The scheduler would crash if a remote IPP queue was stopped (Issue #2460)
- The scheduler did not allow &quot;DefaultAuthType None&quot;.
