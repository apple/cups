README - CUPS v1.4b1 - 2008-10-10
---------------------------------

**********************************************************************
**********************************************************************
**********                                                  **********
**********  THIS IS BETA-RELEASE SOFTWARE.  DO NOT USE ON   **********
**********  PRODUCTION SYSTEMS!  REPORT PROBLEMS TO THE     **********
**********  CUPS FORUMS OR BUG REPORTING PAGES:             **********
**********                                                  **********
**********    http://www.cups.org/newsgroups.php (FORUMS)   **********
**********    http://www.cups.org/str.php        (BUGS)     **********
**********                                                  **********
**********************************************************************
**********************************************************************

Looking for compile instructions?  Read the file "INSTALL.txt"
instead...


INTRODUCTION

    CUPS provides a portable printing layer for UNIX(r)-based
    operating systems.  It was developed by Easy Software Products
    and is now owned and maintained by Apple Inc. to promote a
    standard printing solution for all UNIX vendors and users.  CUPS
    provides the System V and Berkeley command-line interfaces.

    CUPS uses the Internet Printing Protocol ("IPP") as the basis
    for managing print jobs and queues.  The Line Printer Daemon
    ("LPD") Server Message Block ("SMB"), and AppSocket (a.k.a.
    JetDirect) protocols are also supported with reduced
    functionality.  CUPS adds network printer browsing and
    PostScript Printer Description ("PPD") based printing options
    to support real-world printing under UNIX.

    CUPS includes an image file RIP that supports printing of
    image files to non-PostScript printers.  GPL Ghostscript now
    includes the "cups" driver to support printing of PostScript
    files within the CUPS driver framework.  Sample drivers for
    Dymo, EPSON, HP, OKIDATA, and Zebra printers are included that
    use these filters.

    CUPS is licensed under the GNU General Public License and GNU
    Library General Public License versions 2.


READING THE DOCUMENTATION

    Once you have installed the software you can access the
    documentation (and a bunch of other stuff) on-line at:

	http://localhost:631/

    If you're having trouble getting that far, the documentation
    is located under the "doc/help" directory.

    Please read the documentation before asking questions.


GETTING SUPPORT AND OTHER RESOURCES

    If you have problems, READ THE DOCUMENTATION FIRST!  We also
    provide many discussion forums which are available at:

	http://www.cups.org/newsgroups.php

    See the CUPS web site at "http://www.cups.org/" for other
    site links.


SETTING UP PRINTER QUEUES USING YOUR WEB BROWSER

    CUPS 1.3 includes a web-based administration tool that allows
    you to manage printers, classes, and jobs on your server. 
    Open the following URL in your browser to access the printer
    administration tools:

	http://localhost:631/admin/

    DO NOT use the hostname for your machine - it will not work
    with the default CUPS configuration.  To enable
    administration access on other addresses, check the "Allow
    Remote Administration" box and click on the "Change Settings"
    button.

    You will be asked for the administration password (root or
    any other user in the sys/system/root group on your system)
    when performing any administrative function.


SETTING UP PRINTER QUEUES FROM THE COMMAND-LINE

    CUPS works best with PPD (PostScript Printer Description)
    files.  In a pinch you can also use System V style printer
    interface scripts.

    CUPS includes several sample PPD files you can use:

	Driver                         PPD File
	-----------------------------  ------------------------------
	Dymo Label Printers            drv:///sample.drv/dymo.ppd
	Intellitech Intellibar         drv:///sample.drv/intelbar.ppd
	EPSON Stylus Color Series      drv:///sample.drv/stcolor.ppd
	EPSON Stylus Photo Series      drv:///sample.drv/stphoto.ppd
	EPSON Stylus New Color Series  drv:///sample.drv/stcolor2.ppd
	EPSON Stylus New Photo Series  drv:///sample.drv/stphoto2.ppd
	EPSON 9-pin Series             drv:///sample.drv/epson9.ppd
	EPSON 24-pin Series            drv:///sample.drv/epson24.ppd
	HP DeskJet Series              drv:///sample.drv/deskjet.ppd
	HP LaserJet Series             drv:///sample.drv/laserjet.ppd
	OKIDATA 9-Pin Series           drv:///sample.drv/okidata9.ppd
	OKIDATA 24-Pin Series          drv:///sample.drv/okidat24.ppd
	Zebra CPCL Label Printer       drv:///sample.drv/zebracpl.ppd
	Zebra EPL1 Label Printer       drv:///sample.drv/zebraep1.ppd
	Zebra EPL2 Label Printer       drv:///sample.drv/zebraep2.ppd
	Zebra ZPL Label Printer        drv:///sample.drv/zebra.ppd

    Run the "lpinfo -m" command to list the available drivers:

        lpinfo -m

    Run the "lpinfo -v" command to list the available printers:

        lpinfo -v

    Then use the correct URI to add the printer using the
    "lpadmin" command:

        lpadmin -p printername -E -v URI -m filename.ppd

    Network printers typically use "socket" or "lpd" URIs:

        lpadmin -p printername -E -v socket://11.22.33.44 -m filename.ppd
        lpadmin -p printername -E -v lpd://11.22.33.44/ -m filename.ppd

    The sample drivers provide basic printing capabilities, but
    generally do not exercise the full potential of the printers
    or CUPS.


PRINTING FILES

    CUPS provides both the System V "lp" and Berkeley "lpr"
    commands for printing:

	lp filename
	lpr filename

    Both the "lp" and "lpr" commands support printing options for
    the driver:

	lp -omedia=A4 -oresolution=600dpi filename
	lpr -omedia=A4 -oresolution=600dpi filename

    CUPS recognizes many types of images files as well as PDF,
    PostScript, HP-GL/2, and text files, so you can print those
    files directly rather than through an application.

    If you have an application that generates output specifically
    for your printer then you need to use the "-oraw" or "-l"
    options:

	lp -oraw filename
	lpr -l filename

    This will prevent the filters from misinterpreting your print
    file.


LEGAL STUFF

    CUPS is Copyright 2007-2008 by Apple Inc.  CUPS, the CUPS logo,
    and the Common UNIX Printing System are trademarks of Apple Inc.

    The MD5 Digest code is Copyright 1999 Aladdin Enterprises.

    This software is based in part on the work of the Independent
    JPEG Group.

    CUPS is provided under the terms of version 2 of the GNU General
    Public License and GNU Library General Public License. This program
    is distributed in the hope that it will be useful, but WITHOUT ANY
    WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE.  See the "doc/help/license.html"
    or "LICENSE.txt" files for more information.
