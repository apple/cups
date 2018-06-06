---
title: Demystifying CUPS Development
layout: post
permalink: /blog/:year-:month-:day-:title.html
excerpt_separator: <!--more-->
---

We often get questions about CUPS development, the different versions of CUPS,
and the timelines for changes that we have announced.  This article attempts to
answer some of those questions and provide some context for the changes that are
coming for CUPS.

<!--more-->

The Basics
==========

The CUPS project is hosted on [Github](https://github.com/apple/cups) and uses
the [Github issue tracker](https://github.com/apple/cups/issues) to allow anyone
to get the CUPS source code, report a bug in the CUPS software, or ask for a
new feature.

CUPS development happens on two branches - the stable CUPS 2.2.x branch
(Git "branch-2.2") and the CUPS 2.3 feature branch (Git "master").  Bug fixes
are applied to both branches and included in new releases posted on the CUPS web
site.  Features are limited to CUPS 2.3 unless the feature needed for a 2.2.x
bug fix.

For example, the most recent CUPS 2.2.8 and CUPS 2.3b5 releases were purely bug
fix releases.  Prior to that CUPS 2.2.7 included bug fixes plus support for new
`SSLOptions` values to address a security bug, while CUPS 2.3b4 included all of
the changes from CUPS 2.2.7 as well as a new API only for CUPS 2.3.


Common Questions
================

Licensing Changes
-----------------

CUPS 2.2.x and earlier use a license based on the GNU General Public License
version 2 (GPL2) and GNU Library General Public License version 2 (LGPL2) with
exceptions for linking on Apple operating systems.  Unfortunately the GPL2 and
LGPL2 are not generally compatible with newer open source/free software
licenses, so for CUPS 2.3 and later we are using the Apache License Version 2.0
instead.

> Note: Apple is currently working to address the known incompatibility between
> the Apache and GPL2 licenses and will not release CUPS 2.3 until we have
> resolved that issue.


Printer Drivers and Why Apple is Deprecating Them
-------------------------------------------------

CUPS printer drivers are a combination of PostScript Printer Description ("PPD")
files, which describe the unique capabilities of a printer, and any software
needed to communicate with and generate page data for the printer.  Because the
software must be compiled and packaged for every operating system, architecture,
and Linux distribution, many printers have gone unsupported on "unpopular"
systems.  In addition, Adobe last updated the PPD specification in 1996 and
never designed the format to support anything other than the PostScript printers
of that era.

Recognizing these issues, Apple deprecated PPD files in CUPS 1.4 (2008) and
began working on a replacement based on the Internet Printing Protocol ("IPP")
which has been at the core of CUPS since development began at Easy Software
Products in 1997.  Working with the
[Printer Working Group](https://www.pwg.org/ipp), Apple helped to define new
printing standards including IPP 2.0 and
[IPP Everywhere™](https://www.pwg.org/ipp/everywhere.html) to ensure that IPP
could provide all of the functionality normally offered through printer drivers.
In parallel, Apple started a (free) licensing and certification program for
IPP-based iOS and macOS printing called
[AirPrint™](https://support.apple.com/en-ca/ht201311).

The CUPS 1.6 feature release (2010) introduced new printing APIs to allow
applications to easily provide options via IPP, and these APIs have been
extended as needed to address deficiencies as they are reported.  In addition, a
generic IPP-based printer driver was added to CUPS which eliminates the need for
a driver from the manufacturer or third-party developer.  The result of 10 years
of hard work by Apple and every printer manufacturer is that most printers sold
since 2010 (an estimated 600 million of which are still in service) support IPP
printing with standard file formats and can be used without printer drivers or
PPD files with CUPS today.

So, starting with the CUPS 2.3 feature release we are deprecating printer
drivers.  As I mentioned in a
[previous post](/blog/2018-03-27-deprecation.html), *this does not mean that
printer drivers will stop working in CUPS 2.3*.  Rather, we want to give everyone
plenty of notice and work with vendors and developers that, for whatever reason,
still need to use custom software to talk to a printer.  The replacement for
printer drivers is something we are calling *printer applications*, which look
like IPP printers and are discussed in some detail in the
[CUPS Plenary](https://ftp.pwg.org/pub/pwg/liaison/openprinting/presentations/cups-plenary-may-18.pdf)
slides from the May 2018 Open Printing summit.  The `ippserver` sample code
shows how to implement an IPP printer in a single source file, so we are
confident that developers will be able to easily adopt the printer application
approach, particularly for drivers based on the CUPS raster format.


Raw Print Queues and Why Apple is Deprecating Them
--------------------------------------------------

Raw print queues are often used to support queuing of print jobs from
applications that produce print-ready files, often PCL or some label format.
This approach has been used since the earliest days of UNIX, CP/M, and MS-DOS,
but requires each application to support various printer-specific formats and
every user to configure every application in order to print.  One of the design
goals of CUPS was to eliminate the need for raw printing, and to provide a
common interface and print file format to all applications so that UNIX
applications and users could enjoy the same benefits as those on macOS and
Microsoft Windows which likewise provide a common printing interface.

Today the only printers that "need" a raw printing interface are dedicated
label, receipt, or logging printers, and those same printers are poorly served
by CUPS because the applications using them assume they have exclusive access
to the printer, send a single "page" of arbitrary length that is not known ahead
of time, and/or have specific timing requirements that CUPS cannot guarantee.

The other use of raw print queues has been to point a local CUPS print queue to
a remote (shared) CUPS print queue.  This configuration used to be supported but
required the local application to fetch the PPD file from the remote server.
Since sandboxing and other security technologies often prevent an application
from making arbitrary network connections, this configuration cannot be
supported.

Apple deprecated raw print queues in CUPS 2.2.7 (2018) and will remove support
for raw print queues in the feature release after CUPS 2.3 (probably 2020).
