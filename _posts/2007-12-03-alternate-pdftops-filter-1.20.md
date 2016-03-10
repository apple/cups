---
title: alternate pdftops filter 1.20
layout: post
permalink: /blog/:year-:month-:day-:title.html
---

The Perl script is a wrapper that wraps either the pdftops utility of the xpdf (3.0x) suite or the Adobe reader (acroread) to act as a CUPS filter.

The Gentoo bug # 201042 (insecure tempfile creation) has been fixed
by using Perl's File::Temp package.

See the comments at the top of the script for details.
