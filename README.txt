README - CUPS v1.0b1

CONTENTS

   * Introduction
   * Requirements
   * Compiling CUPS
   * Configuring the Software
   * Running the Software
   * Using the Software
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

COMPILING CUPS

CUPS uses GNU autoconf to configure the makefiles and source code for your
system. To configure CUPS for your system type:

     % ./configure ENTER

The default installation will put the CUPS software in the /usr and /var
directories on your system, which will overwrite any existing printing
commands on your system. To install the CUPS software in another location
use the --prefix option:

     % ./configure --prefix=/usr/local ENTER

Once you have configured things, just type:

     % make ENTER

to build the software.

INSTALLING THE SOFTWARE

To install the software type:

     % make install ENTER

CONFIGURING THE SOFTWARE

Before you run CUPS for the first time you'll need to edit the CUPS
configuration files which are normally located in /var/cups/conf.

     The cupsd.conf file configures all of the "global" server settings and
     access control. The default settings are usually appropriate for most
     environments.

     The printers.conf file configures each printer queue. You'll need to
     add a listing for each printer on your system.

     The classes.conf file configures each printer class. You'll need to add
     a listing for each printer class you want.

     The mime.types file defines all of the recognized file types. You don't
     normally have to edit this file.

     The mime.convs file defines all of the file conversion filters. You
     don't normally have to edit this file.

In addition to the files in the /var/cups/conf directory, you'll also need
to copy PPD files for each printer to the /var/cups/ppd directory. If you
don't have a PPD file for your printer, the drivers will still work, just
with reduced functionality.

RUNNING THE SOFTWARE

Once you have configured the software you can start the CUPS daemon by
typing:

     % /usr/sbin/cupsd & ENTER

USING THE SOFTWARE

Once you have installed the software, you can use the normal lp or lpr
commands to print jobs. If you installed the software under /usr then you
shouldn't have to reconfigure any applications to recognize the new printing
system.

One of the advantages of CUPS is that you don't always have to send
PostScript or Text files to your printers. If you have a JPEG file, you can
just type "lp filename.jpg" and CUPS will handle converting it for you!

You can monitor the status of jobs via the lpstat command or with your web
browser by pointing it at "http://localhost:631".

KNOWN PROBLEMS

The following known problems are being worked on and should be resolved for
the second beta release of CUPS:

   * Documentation is not completed.
   * The lpadmin command is currently not provided.
   * The lpq command is currently not provided.
   * The lpc command currently only supports the help and status commands.
   * While both GNU GhostScript and the CUPS image RIP are provided, no
     sample raster printer driver is provided. The final release of CUPS
     will include a PCL printer driver.
   * Automatic classing is currently not supported.
   * The CUPS server should disable core dumps by filters, backends, and CGI
     programs.
   * The CUPS server should increase the FD limit to the maximum allowed on
     the system.
   * The CUPS server should close stdin, stdout, and stderr and run in the
     background ("daemon" mode...)
   * The class and job CGIs are currently not provided.

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

CUPS is provided under the terms of the Aladdin Free Public License which is
located in the files "LICENSE.html" and "LICENSE.txt". For commercial
licensing information, please contact:

     Attn: CUPS Licensing Information
     Easy Software Products
     44141 Airport View Drive, Suite 204
     Hollywood, Maryland 20636-3111 USA

     Voice: +1.301.373.9603
     Email: cups-info@cups.org
     WWW: http://www.cups.org

If you're interested in a complete, commercial printing solution for UNIX,
check out our ESP Print software at http://www.easysw.com/print.html.
