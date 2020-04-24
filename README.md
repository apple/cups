README - CUPS v2.3.3 - 2020-04-24
=================================

INTRODUCTION
------------

CUPS is a standards-based, open source printing system developed by Apple Inc.
for macOS® and other UNIX®-like operating systems.  CUPS uses the Internet
Printing Protocol ("IPP") and provides System V and Berkeley command-line
interfaces, a web interface, and a C API to manage printers and print jobs.  It
supports printing to both local (parallel, serial, USB) and networked printers,
and printers can be shared from one computer to another, even over the Internet!

Internally, CUPS uses PostScript Printer Description ("PPD") files to describe
printer capabilities and features and a wide variety of generic and device-
specific programs to convert and print many types of files.  Sample drivers are
included with CUPS to support many Dymo, EPSON, HP, Intellitech, OKIDATA, and
Zebra printers.  Many more drivers are available online and (in some cases) on
the driver CD-ROM that came with your printer.

CUPS is licensed under the Apache License Version 2.0.  See the file
"LICENSE" for more information.


READING THE DOCUMENTATION
-------------------------

Initial documentation to get you started is provided in the root directory of
the CUPS sources:

- `CHANGES.md`: A list of changes in the current major release of CUPS.
- `CONTRIBUTING.md`: Guidelines for contributing to the CUPS project.
- `CREDITS.md`: A list of past contributors to the CUPS project.
- `DEVELOPING.md`: Guidelines for developing code for the CUPS project.
- `INSTALL.md`: Instructions for building and installing CUPS.
- `LICENSE`: The CUPS license agreement (Apache 2.0).
- `NOTICE`: Copyright notices and exceptions to the CUPS license agreement.
- `README.md`: This file.

Once you have installed the software you can access the documentation (and a
bunch of other stuff) online at <http://localhost:631/> and using the `man`
command, for example `man cups`.

If you're having trouble getting that far, the documentation is located under
the `doc/help` and `man` directories.

Please read the documentation before asking questions.


GETTING SUPPORT AND OTHER RESOURCES
-----------------------------------

If you have problems, *read the documentation first!*  We also provide two
mailing lists which are available at <https://lists.cups.org/mailman/listinfo>.

See the CUPS web site at <https://www.cups.org/> for other resources.


SETTING UP PRINTER QUEUES USING YOUR WEB BROWSER
------------------------------------------------

CUPS includes a web-based administration tool that allows you to manage
printers, classes, and jobs on your server.  Open <http://localhost:631/admin/>
in your browser to access the printer administration tools:

*Do not* use the hostname for your machine - it will not work with the default
CUPS configuration.  To enable administration access on other addresses, check
the `Allow Remote Administration` box and click on the `Change Settings` button.

You will be asked for the administration password (root or any other user in the
"sys", "system", "root", "admin", or "lpadmin" group on your system) when
performing any administrative function.


SETTING UP PRINTER QUEUES FROM THE COMMAND-LINE
-----------------------------------------------

CUPS currently uses PPD (PostScript Printer Description) files that describe
printer capabilities and driver programs needed for each printer.  The
`everywhere` PPD is used for nearly all modern networks printers sold since
about 2009.  For example, the following command creates a print queue for a
printer at address "11.22.33.44":

    lpadmin -p printername -E -v ipp://11.22.33.44/ipp/print -m everywhere

CUPS also includes several sample PPD files you can use for "legacy" printers:

   Driver                         | PPD Name
   -----------------------------  | ------------------------------
   Dymo Label Printers            | drv:///sample.drv/dymo.ppd
   Intellitech Intellibar         | drv:///sample.drv/intelbar.ppd
   EPSON 9-pin Series             | drv:///sample.drv/epson9.ppd
   EPSON 24-pin Series            | drv:///sample.drv/epson24.ppd
   Generic PCL Laser Printer      | drv:///sample.drv/generpcl.ppd
   Generic PostScript Printer     | drv:///sample.drv/generic.ppd
   HP DeskJet Series              | drv:///sample.drv/deskjet.ppd
   HP LaserJet Series             | drv:///sample.drv/laserjet.ppd
   OKIDATA 9-Pin Series           | drv:///sample.drv/okidata9.ppd
   OKIDATA 24-Pin Series          | drv:///sample.drv/okidat24.ppd
   Zebra CPCL Label Printer       | drv:///sample.drv/zebracpl.ppd
   Zebra EPL1 Label Printer       | drv:///sample.drv/zebraep1.ppd
   Zebra EPL2 Label Printer       | drv:///sample.drv/zebraep2.ppd
   Zebra ZPL Label Printer        | drv:///sample.drv/zebra.ppd

You can run the `lpinfo -m` command to list all of the available drivers:

    lpinfo -m

Run the `lpinfo -v` command to list the available printers:

    lpinfo -v

Then use the correct URI to add the printer using the `lpadmin` command:

    lpadmin -p printername -E -v device-uri -m ppd-name

Current network printers typically use `ipp` or `ipps` URIS:

    lpadmin -p printername -E -v ipp://11.22.33.44/ipp/print -m everywhere
    lpadmin -p printername -E -v ipps://11.22.33.44/ipp/print -m everywhere

Older network printers typically use `socket` or `lpd` URIs:

    lpadmin -p printername -E -v socket://11.22.33.44 -m ppd-name
    lpadmin -p printername -E -v lpd://11.22.33.44/ -m ppd-name

The sample drivers provide basic printing capabilities, but generally do not
exercise the full potential of the printers or CUPS.  Other drivers provide
greater printing capabilities.


PRINTING FILES
--------------

CUPS provides both the System V `lp` and Berkeley `lpr` commands for printing:

    lp filename
    lpr filename

Both the `lp` and `lpr` commands support printing options for the driver:

    lp -o media=A4 -o resolution=600dpi filename
    lpr -o media=A4 -o resolution=600dpi filename

CUPS recognizes many types of images files as well as PDF, PostScript, and text
files, so you can print those files directly rather than through an application.

If you have an application that generates output specifically for your printer
then you need to use the `-oraw` or `-l` options:

    lp -o raw filename
    lpr -l filename

This will prevent the filters from misinterpreting your print file.


LEGAL STUFF
-----------

Copyright © 2007-2019 by Apple Inc.
Copyright © 1997-2007 by Easy Software Products.

CUPS is provided under the terms of the Apache License, Version 2.0 with
exceptions for GPL2/LGPL2 software.  A copy of this license can be found in the
file `LICENSE`.  Additional legal information is provided in the file `NOTICE`.

Unless required by applicable law or agreed to in writing, software distributed
under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
CONDITIONS OF ANY KIND, either express or implied.  See the License for the
specific language governing permissions and limitations under the License.
