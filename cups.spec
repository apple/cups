#
# "$Id: cups.spec,v 1.6 1999/09/27 18:24:36 mike Exp $"
#
#   RPM "spec" file for the Common UNIX Printing System (CUPS).
#
#   Original version by Jason McMullan <jmcc@ontv.com>.
#
#   Copyright 1999 by Easy Software Products, all rights reserved.
#
#   These coded instructions, statements, and computer programs are the
#   property of Easy Software Products and are protected by Federal
#   copyright law.  Distribution and use rights are outlined in the file
#   "LICENSE.txt" which should have been included with this file.  If this
#   file is missing or damaged please contact Easy Software Products
#   at:
#
#       Attn: CUPS Licensing Information
#       Easy Software Products
#       44141 Airport View Drive, Suite 204
#       Hollywood, Maryland 20636-3111 USA
#
#       Voice: (301) 373-9603
#       EMail: cups-info@cups.org
#         WWW: http://www.cups.org
#

Summary: Common Unix Printing System
Name: cups
Version: 1.0b10
Release: 0
Copyright: GPL
Group: System Environment/Daemons
Source: ftp://ftp.easysw.com/pub/cups/beta/cups-1.0b10-source.tar.gz
Url: http://www.cups.org
Packager: Michael Sweet <mike@easysw.com>
Vendor: Easy Software Products
# use buildroot so as not to disturb the version already installed
BuildRoot: /tmp/rpmbuild
Conflicts: lpr

%package devel
Summary: Common Unix Printing System - development environment
Group: Development/Libraries

%description
The Common UNIX Printing System provides a portable printing layer for 
UNIX® operating systems. It has been developed by Easy Software Products 
to promote a standard printing solution for all UNIX vendors and users. 
CUPS provides the System V and Berkeley command-line interfaces. 

%description devel
The Common UNIX Printing System provides a portable printing layer for 
UNIX® operating systems. This is the development package for creating
additional printer drivers, and other CUPS services.

%prep
%setup

%build
./configure

# If we got this far, all prerequisite libraries must be here.
make

%install
# these lines just make sure the directory structure in the
# RPM_BUILD_ROOT exists
rm -rf $RPM_BUILD_ROOT
mkdir -p $RPM_BUILD_ROOT/etc
mkdir -p $RPM_BUILD_ROOT/etc/rc.d
mkdir -p $RPM_BUILD_ROOT/etc/rc.d/init.d
mkdir -p $RPM_BUILD_ROOT/usr
mkdir -p $RPM_BUILD_ROOT/usr/bin
mkdir -p $RPM_BUILD_ROOT/usr/lib
mkdir -p $RPM_BUILD_ROOT/usr/man
mkdir -p $RPM_BUILD_ROOT/usr/man/man1
mkdir -p $RPM_BUILD_ROOT/usr/man/man5
mkdir -p $RPM_BUILD_ROOT/usr/man/man8
mkdir -p $RPM_BUILD_ROOT/usr/share/locale
mkdir -p $RPM_BUILD_ROOT/var/cups
mkdir -p $RPM_BUILD_ROOT/var/cups/conf
mkdir -p $RPM_BUILD_ROOT/var/cups/logs
mkdir -p $RPM_BUILD_ROOT/var/logs

ln -sf /var/cups/logs $RPM_BUILD_ROOT/var/logs/cups
ln -sf /var/cups/conf $RPM_BUILD_ROOT/etc/cups

make prefix=$RPM_BUILD_ROOT/usr DATADIR=$RPM_BUILD_ROOT/usr/share/cups LOCALEDIR=$RPM_BUILD_ROOT/usr/share/locale SERVERROOT=$RPM_BUILD_ROOT/var/cups install

$RPM_BUILD_ROOT/etc/rc.d/init.d/cups
install -m 755 -o root -g root cups.sh $RPM_BUILD_ROOT/etc/rc.d/init.d/cups

%post
/sbin/chkconfig --add cups

%preun
/sbin/chkconfig --del cups

%clean
rm -rf $RPM_BUILD_ROOT

%files
/etc/rc.d/init.d/cups
%config /var/cups/conf/classes.conf
%config /var/cups/conf/cupsd.conf
%config /var/cups/conf/mime.convs
%config /var/cups/conf/mime.types
%config /var/cups/conf/printers.conf
/usr/bin/lpr
/usr/bin/lprm
/usr/bin/disable
/usr/bin/enable
/usr/bin/cancel
/usr/bin/lp
/usr/bin/lpstat
/usr/lib/accept
/usr/lib/libcups.so.1
/usr/lib/libcupsimage.so.1
/usr/lib/lpadmin
/usr/lib/reject
/usr/man/man1/backend.1
/usr/man/man1/filter.1
/usr/man/man1/lprm.1
/usr/man/man1/lpr.1
/usr/man/man1/lpstat.1
/usr/man/man1/lp.1
/usr/man/man1/cancel.1
/usr/man/man5/classes.conf.5
/usr/man/man5/cupsd.conf.5
/usr/man/man5/mime.convs.5
/usr/man/man5/mime.types.5
/usr/man/man5/printers.conf.5
/usr/man/man8/accept.8
/usr/man/man8/cupsd.8
/usr/man/man8/enable.8
/usr/man/man8/lpadmin.8
/usr/man/man8/lpc.8
/usr/man/man8/reject.8
/usr/man/man8/disable.8
/usr/sbin/accept
/usr/sbin/cupsd
/usr/sbin/lpadmin
/usr/sbin/lpc
/usr/sbin/reject
%dir /usr/share/cups
/usr/share/cups/data/8859-1
/usr/share/cups/data/8859-14
/usr/share/cups/data/8859-15
/usr/share/cups/data/8859-2
/usr/share/cups/data/8859-3
/usr/share/cups/data/8859-4
/usr/share/cups/data/8859-5
/usr/share/cups/data/8859-6
/usr/share/cups/data/8859-7
/usr/share/cups/data/8859-8
/usr/share/cups/data/8859-9
/usr/share/cups/data/HPGLprolog
/usr/share/cups/data/psglyphs
/usr/share/cups/doc/cmp.html
/usr/share/cups/doc/cmp.pdf
/usr/share/cups/doc/cups.css
/usr/share/cups/doc/cupsdoc.css
/usr/share/cups/doc/documentation.html
/usr/share/cups/doc/idd.html
/usr/share/cups/doc/idd.pdf
/usr/share/cups/doc/images/classes.gif
/usr/share/cups/doc/images/cups-block-diagram.gif
/usr/share/cups/doc/images/cups-large.gif
/usr/share/cups/doc/images/cups-medium.gif
/usr/share/cups/doc/images/cups-small.gif
/usr/share/cups/doc/images/logo.gif
/usr/share/cups/doc/images/navbar.gif
/usr/share/cups/doc/images/printer-idle.gif
/usr/share/cups/doc/images/printer-processing.gif
/usr/share/cups/doc/images/printer-stopped.gif
/usr/share/cups/doc/index.html
/usr/share/cups/doc/overview.html
/usr/share/cups/doc/overview.pdf
/usr/share/cups/doc/sam.html
/usr/share/cups/doc/sam.pdf
/usr/share/cups/doc/sdd.html
/usr/share/cups/doc/sdd.pdf
/usr/share/cups/doc/ssr.html
/usr/share/cups/doc/ssr.pdf
/usr/share/cups/doc/stp.html
/usr/share/cups/doc/stp.pdf
/usr/share/cups/doc/sum.html
/usr/share/cups/doc/sum.pdf
/usr/share/cups/doc/svd.html
/usr/share/cups/doc/svd.pdf
/usr/share/cups/fonts/AvantGarde-Book
/usr/share/cups/fonts/AvantGarde-BookOblique
/usr/share/cups/fonts/AvantGarde-Demi
/usr/share/cups/fonts/AvantGarde-DemiOblique
/usr/share/cups/fonts/Bookman-Demi
/usr/share/cups/fonts/Bookman-DemiItalic
/usr/share/cups/fonts/Bookman-Light
/usr/share/cups/fonts/Bookman-LightItalic
/usr/share/cups/fonts/Courier
/usr/share/cups/fonts/Courier-Bold
/usr/share/cups/fonts/Courier-BoldOblique
/usr/share/cups/fonts/Courier-Oblique
/usr/share/cups/fonts/Helvetica
/usr/share/cups/fonts/Helvetica-Bold
/usr/share/cups/fonts/Helvetica-BoldOblique
/usr/share/cups/fonts/Helvetica-Narrow
/usr/share/cups/fonts/Helvetica-Narrow-Bold
/usr/share/cups/fonts/Helvetica-Narrow-BoldOblique
/usr/share/cups/fonts/Helvetica-Narrow-Oblique
/usr/share/cups/fonts/Helvetica-Oblique
/usr/share/cups/fonts/NewCenturySchlbk-Bold
/usr/share/cups/fonts/NewCenturySchlbk-BoldItalic
/usr/share/cups/fonts/NewCenturySchlbk-Italic
/usr/share/cups/fonts/NewCenturySchlbk-Roman
/usr/share/cups/fonts/Palatino-Bold
/usr/share/cups/fonts/Palatino-BoldItalic
/usr/share/cups/fonts/Palatino-Italic
/usr/share/cups/fonts/Palatino-Roman
/usr/share/cups/fonts/Symbol
/usr/share/cups/fonts/Times-Bold
/usr/share/cups/fonts/Times-BoldItalic
/usr/share/cups/fonts/Times-Italic
/usr/share/cups/fonts/Times-Roman
/usr/share/cups/fonts/Utopia-Bold
/usr/share/cups/fonts/Utopia-BoldItalic
/usr/share/cups/fonts/Utopia-Italic
/usr/share/cups/fonts/Utopia-Regular
/usr/share/cups/fonts/ZapfChancery-MediumItalic
/usr/share/cups/fonts/ZapfDingbats
/usr/share/cups/model/deskjet.ppd
/usr/share/cups/model/laserjet.ppd
/usr/share/cups/pstoraster/Fontmap
/usr/share/cups/pstoraster/gs_btokn.ps
/usr/share/cups/pstoraster/gs_ccfnt.ps
/usr/share/cups/pstoraster/gs_cidfn.ps
/usr/share/cups/pstoraster/gs_cmap.ps
/usr/share/cups/pstoraster/gs_cmdl.ps
/usr/share/cups/pstoraster/gs_dbt_e.ps
/usr/share/cups/pstoraster/gs_diskf.ps
/usr/share/cups/pstoraster/gs_dps1.ps
/usr/share/cups/pstoraster/gs_fform.ps
/usr/share/cups/pstoraster/gs_fonts.ps
/usr/share/cups/pstoraster/gs_init.ps
/usr/share/cups/pstoraster/gs_iso_e.ps
/usr/share/cups/pstoraster/gs_kanji.ps
/usr/share/cups/pstoraster/gs_ksb_e.ps
/usr/share/cups/pstoraster/gs_l2img.ps
/usr/share/cups/pstoraster/gs_lev2.ps
/usr/share/cups/pstoraster/gs_mex_e.ps
/usr/share/cups/pstoraster/gs_mro_e.ps
/usr/share/cups/pstoraster/gs_pdf.ps
/usr/share/cups/pstoraster/gs_pdf_e.ps
/usr/share/cups/pstoraster/gs_pdfwr.ps
/usr/share/cups/pstoraster/gs_pfile.ps
/usr/share/cups/pstoraster/gs_res.ps
/usr/share/cups/pstoraster/gs_setpd.ps
/usr/share/cups/pstoraster/gs_statd.ps
/usr/share/cups/pstoraster/gs_std_e.ps
/usr/share/cups/pstoraster/gs_sym_e.ps
/usr/share/cups/pstoraster/gs_ttf.ps
/usr/share/cups/pstoraster/gs_typ42.ps
/usr/share/cups/pstoraster/gs_type1.ps
/usr/share/cups/pstoraster/gs_wan_e.ps
/usr/share/cups/pstoraster/gs_wl1_e.ps
/usr/share/cups/pstoraster/gs_wl2_e.ps
/usr/share/cups/pstoraster/gs_wl5_e.ps
/usr/share/cups/pstoraster/pdf_2ps.ps
/usr/share/cups/pstoraster/pdf_base.ps
/usr/share/cups/pstoraster/pdf_draw.ps
/usr/share/cups/pstoraster/pdf_font.ps
/usr/share/cups/pstoraster/pdf_main.ps
/usr/share/cups/pstoraster/pdf_sec.ps
/usr/share/cups/pstoraster/pfbtogs.ps
%dir /var/cups
/var/cups/backend/http
/var/cups/backend/ipp
/var/cups/backend/lpd
/var/cups/backend/parallel
/var/cups/backend/serial
/var/cups/backend/socket
/var/cups/cgi-bin/classes.cgi
/var/cups/cgi-bin/jobs.cgi
/var/cups/cgi-bin/printers.cgi
/var/cups/conf
/var/cups/filter/hpgltops
/var/cups/filter/imagetops
/var/cups/filter/imagetoraster
/var/cups/filter/pstops
/var/cups/filter/pstoraster
/var/cups/filter/rastertohp
/var/cups/filter/texttops
%dir /var/cups/interfaces
%dir /var/cups/logs
%dir /var/cups/ppd
%dir /var/cups/requests

%files devel
%dir /usr/include/cups
/usr/include/cups/cups.h
/usr/include/cups/http.h
/usr/include/cups/ipp.h
/usr/include/cups/language.h
/usr/include/cups/mime.h
/usr/include/cups/ppd.h
/usr/include/cups/raster.h

#
# End of "$Id: cups.spec,v 1.6 1999/09/27 18:24:36 mike Exp $".
#
