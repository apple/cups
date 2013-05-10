README - CUPS v1.0b4
--------------------

CONTENTS

   * Introduction
   * Requirements
   * Using CUPS
   * Known Problems
   * Reporting Problems
   * Other Resources
   * Legal Stuff


INTRODUCTION

The Common UNIX Printing System provides a portable printing layer for UNIX®
operating systems. It has been developed by Easy Software Products to
promote a standard printing solution for all UNIX vendors and users. CUPS
provides the System V and Berkeley command-line interfaces.

CUPS uses the Internet Printing Protocol (IETF-IPP) as the basis for
managing print jobs and queues. The Line Printer Daemon (LPD, RFC1179),
Server Message Block (SMB), and AppSocket protocols are also supported with
reduced functionality.

CUPS adds network printer browsing and PostScript Printer Description
("PPD")-based printing options to support real world applications under
UNIX.

CUPS also includes a customized version of GNU GhostScript (currently based
off GNU GhostScript 4.03) and an image file RIP that can be used to support
non-PostScript printers.


REQUIREMENTS

You'll need an ANSI C compiler to build CUPS on your system. As its name
implies, CUPS is designed to run on the UNIX operating system, however the
CUPS interface library and most of the filters and backends supplied with
CUPS should also run under Microsoft® Windows®.

For the image file filters you'll need the JPEG, PNG, TIFF, and ZLIB
libraries. CUPS will build without these, but with reduced functionality.

If you make changes to the man pages you'll need GNU groff or another
nroff-like package.

The documentation is formatted using the HTMLDOC software (again, not needed
unless you make changes.)


USING CUPS

Pre-compiled binary distributions are available for CUPS from our web site
at http://www.cups.org/software.html. If you'd like to build CUPS from the
source, please read the Software Administrator's Manual.

Once you have installed CUPS, the Software Administrator's Manual and
Software User's Manual are excellent places to start setting things up.


KNOWN PROBLEMS

The following known problems are being worked on and should be resolved for
the fourth beta release of CUPS:

   * Documentation is not completed.
   * The lpc command currently only supports the help and status commands.

CUPS has been built and tested on the following operating systems:

   * Digital UNIX 4.0d
   * HP-UX 10.20 and 11.0
   * IRIX 5.3, 6.2, 6.5.3
   * Linux (RedHat 5.2)
   * Solaris 2.5.1, 2.6, 2.7 (aka 7)

The client libraries and filters have been successfully compiled under
Microsoft Windows using Visual C++ 6.0.


REPORTING PROBLEMS

If you have problems, please send an email to cups-support@cups.org. Include
your operating system and version, compiler and version, and any errors or
problems you've run into.


OTHER RESOURCES

See the CUPS web site at "http://www.cups.org" for other site links.

You can subscribe to the CUPS mailing list by sending a message containing
"subscribe cups" to majordomo@cups.org. This list is provided to discuss
problems, questions, and improvements to the CUPS software. New releases of
CUPS are announced to this list as well.


LEGAL STUFF

CUPS is Copyright 1993-1999 by Easy Software Products. CUPS, the CUPS logo,
and the Common UNIX Printing System are the trademark property of Easy
Software Products.

CUPS is provided under the terms of the GNU General Public License which is
located in the files "LICENSE.html" and "LICENSE.txt". For commercial
support and "binary-only" licensing information, please contact:

     Attn: CUPS Licensing Information
     Easy Software Products
     44141 Airport View Drive, Suite 204
     Hollywood, Maryland 20636-3111 USA

     Voice: +1.301.373.9603
     Email: cups-info@cups.org
     WWW: http://www.cups.org

If you're interested in a complete, commercial printing solution for UNIX,
check out our ESP Print software at http://www.easysw.com/print.html.
