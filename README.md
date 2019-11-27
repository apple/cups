README - CUPS v2.3.1 - 2019-10-07
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

             Apache License
                           Version 2.0, January 2004
                        https://www.apache.org/licenses/

   TERMS AND CONDITIONS FOR USE, REPRODUCTION, AND DISTRIBUTION

   1. Definitions.

      "License" shall mean the terms and conditions for use, reproduction,
      and distribution as defined by Sections 1 through 9 of this document.

      "Licensor" shall mean the copyright owner or entity authorized by
      the copyright owner that is granting the License.

      "Legal Entity" shall mean the union of the acting entity and all
      other entities that control, are controlled by, or are under common
      control with that entity. For the purposes of this definition,
      "control" means (i) the power, direct or indirect, to cause the
      direction or management of such entity, whether by contract or
      otherwise, or (ii) ownership of fifty percent (50%) or more of the
      outstanding shares, or (iii) beneficial ownership of such entity.

      "You" (or "Your") shall mean an individual or Legal Entity
      exercising permissions granted by this License.

      "Source" form shall mean the preferred form for making modifications,
      including but not limited to software source code, documentation
      source, and configuration files.

      "Object" form shall mean any form resulting from mechanical
      transformation or translation of a Source form, including but
      not limited to compiled object code, generated documentation,
      and conversions to other media types.

      "Work" shall mean the work of authorship, whether in Source or
      Object form, made available under the License, as indicated by a
      copyright notice that is included in or attached to the work
      (an example is provided in the Appendix below).

      "Derivative Works" shall mean any work, whether in Source or Object
      form, that is based on (or derived from) the Work and for which the
      editorial revisions, annotations, elaborations, or other modifications
      represent, as a whole, an original work of authorship. For the purposes
      of this License, Derivative Works shall not include works that remain
      separable from, or merely link (or bind by name) to the interfaces of,
      the Work and Derivative Works thereof.

      "Contribution" shall mean any work of authorship, including
      the original version of the Work and any modifications or additions
      to that Work or Derivative Works thereof, that is intentionally
      submitted to Licensor for inclusion in the Work by the copyright owner
      or by an individual or Legal Entity authorized to submit on behalf of
      the copyright owner. For the purposes of this definition, "submitted"
      means any form of electronic, verbal, or written communication sent
      to the Licensor or its representatives, including but not limited to
      communication on electronic mailing lists, source code control systems,
      and issue tracking systems that are managed by, or on behalf of, the
      Licensor for the purpose of discussing and improving the Work, but
      excluding communication that is conspicuously marked or otherwise
      designated in writing by the copyright owner as "Not a Contribution."

      "Contributor" shall mean Licensor and any individual or Legal Entity
      on behalf of whom a Contribution has been received by Licensor and
      subsequently incorporated within the Work.

   2. Grant of Copyright License. Subject to the terms and conditions of
      this License, each Contributor hereby grants to You a perpetual,
      worldwide, non-exclusive, no-charge, royalty-free, irrevocable
      copyright license to reproduce, prepare Derivative Works of,
      publicly display, publicly perform, sublicense, and distribute the
      Work and such Derivative Works in Source or Object form.

   3. Grant of Patent License. Subject to the terms and conditions of
      this License, each Contributor hereby grants to You a perpetual,
      worldwide, non-exclusive, no-charge, royalty-free, irrevocable
      (except as stated in this section) patent license to make, have made,
      use, offer to sell, sell, import, and otherwise transfer the Work,
      where such license applies only to those patent claims licensable
      by such Contributor that are necessarily infringed by their
      Contribution(s) alone or by combination of their Contribution(s)
      with the Work to which such Contribution(s) was submitted. If You
      institute patent litigation against any entity (including a
      cross-claim or counterclaim in a lawsuit) alleging that the Work
      or a Contribution incorporated within the Work constitutes direct
      or contributory patent infringement, then any patent licenses
      granted to You under this License for that Work shall terminate
      as of the date such litigation is filed.

   4. Redistribution. You may reproduce and distribute copies of the
      Work or Derivative Works thereof in any medium, with or without
      modifications, and in Source or Object form, provided that You
      meet the following conditions:

      (a) You must give any other recipients of the Work or
          Derivative Works a copy of this License; and

      (b) You must cause any modified files to carry prominent notices
          stating that You changed the files; and

      (c) You must retain, in the Source form of any Derivative Works
          that You distribute, all copyright, patent, trademark, and
          attribution notices from the Source form of the Work,
          excluding those notices that do not pertain to any part of
          the Derivative Works; and

      (d) If the Work includes a "NOTICE" text file as part of its
          distribution, then any Derivative Works that You distribute must
          include a readable copy of the attribution notices contained
          within such NOTICE file, excluding those notices that do not
          pertain to any part of the Derivative Works, in at least one
          of the following places: within a NOTICE text file distributed
          as part of the Derivative Works; within the Source form or
          documentation, if provided along with the Derivative Works; or,
          within a display generated by the Derivative Works, if and
          wherever such third-party notices normally appear. The contents
          of the NOTICE file are for informational purposes only and
          do not modify the License. You may add Your own attribution
          notices within Derivative Works that You distribute, alongside
          or as an addendum to the NOTICE text from the Work, provided
          that such additional attribution notices cannot be construed
          as modifying the License.

      You may add Your own copyright statement to Your modifications and
      may provide additional or different license terms and conditions
      for use, reproduction, or distribution of Your modifications, or
      for any such Derivative Works as a whole, provided Your use,
      reproduction, and distribution of the Work otherwise complies with
      the conditions stated in this License.

   5. Submission of Contributions. Unless You explicitly state otherwise,
      any Contribution intentionally submitted for inclusion in the Work
      by You to the Licensor shall be under the terms and conditions of
      this License, without any additional terms or conditions.
      Notwithstanding the above, nothing herein shall supersede or modify
      the terms of any separate license agreement you may have executed
      with Licensor regarding such Contributions.

   6. Trademarks. This License does not grant permission to use the trade
      names, trademarks, service marks, or product names of the Licensor,
      except as required for reasonable and customary use in describing the
      origin of the Work and reproducing the content of the NOTICE file.

   7. Disclaimer of Warranty. Unless required by applicable law or
      agreed to in writing, Licensor provides the Work (and each
      Contributor provides its Contributions) on an "AS IS" BASIS,
      WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
      implied, including, without limitation, any warranties or conditions
      of TITLE, NON-INFRINGEMENT, MERCHANTABILITY, or FITNESS FOR A
      PARTICULAR PURPOSE. You are solely responsible for determining the
      appropriateness of using or redistributing the Work and assume any
      risks associated with Your exercise of permissions under this License.

   8. Limitation of Liability. In no event and under no legal theory,
      whether in tort (including negligence), contract, or otherwise,
      unless required by applicable law (such as deliberate and grossly
      negligent acts) or agreed to in writing, shall any Contributor be
      liable to You for damages, including any direct, indirect, special,
      incidental, or consequential damages of any character arising as a
      result of this License or out of the use or inability to use the
      Work (including but not limited to damages for loss of goodwill,
      work stoppage, computer failure or malfunction, or any and all
      other commercial damages or losses), even if such Contributor
      has been advised of the possibility of such damages.

   9. Accepting Warranty or Additional Liability. While redistributing
      the Work or Derivative Works thereof, You may choose to offer,
      and charge a fee for, acceptance of support, warranty, indemnity,
      or other liability obligations and/or rights consistent with this
      License. However, in accepting such obligations, You may act only
      on Your own behalf and on Your sole responsibility, not on behalf
      of any other Contributor, and only if You agree to indemnify,
      defend, and hold each Contributor harmless for any liability
      incurred by, or claims asserted against, such Contributor by reason
      of your accepting any such warranty or additional liability.

   END OF TERMS AND CONDITIONS

   APPENDIX: How to apply the Apache License to your work.

      To apply the Apache License to your work, attach the following
      boilerplate notice, with the fields enclosed by brackets "[]"
      replaced with your own identifying information. (Don't include
      the brackets!)  The text should be enclosed in the appropriate
      comment syntax for the file format. We also recommend that a
      file or class name and description of purpose be included on the
      same "printed page" as the copyright notice for easier
      identification within third-party archives.

   Copyright 2019 Rolano Gopez Lacuata.

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       https://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.

