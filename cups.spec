#
# "$Id: cups.spec,v 1.30.2.1 2001/05/13 18:37:58 mike Exp $"
#
#   RPM "spec" file for the Common UNIX Printing System (CUPS).
#
#   Original version by Jason McMullan <jmcc@ontv.com>.
#
#   Copyright 1999-2001 by Easy Software Products, all rights reserved.
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
Version: 1.1.7
Release: 1
Copyright: GPL
Group: System Environment/Daemons
Source: ftp://ftp.easysw.com/pub/cups/%{version}/cups-%{version}-source.tar.gz
Url: http://www.cups.org
Packager: Michael Sweet <mike@easysw.com>
Vendor: Easy Software Products
# use buildroot so as not to disturb the version already installed
BuildRoot: /var/tmp/%{name}-root
Conflicts: lpr, LPRng
Provides: libcups.so.2
Provides: libcupsimage.so.2
Provides: cupsd

%package devel
Summary: Common Unix Printing System - development environment
Group: Development/Libraries

%package pstoraster
Summary: Common Unix Printing System - PostScript RIP
Group: System Environment/Daemons
Provides: pstoraster

%description
The Common UNIX Printing System provides a portable printing layer for 
UNIX® operating systems. It has been developed by Easy Software Products 
to promote a standard printing solution for all UNIX vendors and users. 
CUPS provides the System V and Berkeley command-line interfaces. 

%description devel
The Common UNIX Printing System provides a portable printing layer for 
UNIX® operating systems. This is the development package for creating
additional printer drivers, and other CUPS services.

%description devel
The Common UNIX Printing System provides a portable printing layer for 
UNIX® operating systems. This is the PostScript RIP package for
supporting non-PostScript printer drivers.

%prep
%setup

%build
./configure

# If we got this far, all prerequisite libraries must be here.
make

%install
# Make sure the RPM_BUILD_ROOT directory exists.
rm -rf $RPM_BUILD_ROOT

make	prefix=$RPM_BUILD_ROOT \
	exec_prefix=$RPM_BUILD_ROOT/usr \
	AMANDIR=$RPM_BUILD_ROOT/usr/man \
	BINDIR=$RPM_BUILD_ROOT/usr/bin \
	DATADIR=$RPM_BUILD_ROOT/usr/share/cups \
	DOCDIR=$RPM_BUILD_ROOT/usr/share/doc/cups \
	INCLUDEDIR=$RPM_BUILD_ROOT/usr/include \
	LIBDIR=$RPM_BUILD_ROOT/usr/lib \
	LOGDIR=$RPM_BUILD_ROOT/var/log/cups \
	LOCALEDIR=$RPM_BUILD_ROOT/usr/share/locale \
	MANDIR=$RPM_BUILD_ROOT/usr/man \
	PAMDIR=$RPM_BUILD_ROOT/etc/pam.d \
	REQUESTS=$RPM_BUILD_ROOT/var/spool/cups \
	SBINDIR=$RPM_BUILD_ROOT/usr/sbin \
	SERVERBIN=$RPM_BUILD_ROOT/usr/lib/cups \
	SERVERROOT=$RPM_BUILD_ROOT/etc/cups \
	install

%post
if test -x /sbin/chkconfig; then
	/sbin/chkconfig --add cups
	/sbin/chkconfig cups on
fi

# these lines automatically start cupsd after installation; commented out
# by request...
#if test -f /sbin/init.d/cups; then
#	/sbin/init.d/cups start
#fi
#if test -f /etc/rc.d/init.d/cups; then
#	/etc/rc.d/init.d/cups start
#fi
#if test -f /etc/init.d/cups; then
#	/etc/init.d/cups start
#fi

%preun
if test -f /sbin/init.d/cups; then
	/sbin/init.d/cups stop
fi
if test -f /etc/rc.d/init.d/cups; then
	/etc/rc.d/init.d/cups stop
fi
if test -f /etc/init.d/cups; then
	/etc/init.d/cups stop
fi

if test -x /sbin/chkconfig; then
	/sbin/chkconfig --del cups
fi

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root)
%dir /etc/cups
%config(noreplace) /etc/cups/*.conf
%dir /etc/cups/certs
%dir /etc/cups/interfaces
/etc/cups/mime.types
/etc/cups/mime.convs
%dir /etc/cups/ppd
%dir /etc/pam.d
/etc/pam.d/*

# RC dirs are a pain under Linux...  Uncomment the appropriate ones if you
# don't use Red Hat or Mandrake...

/etc/rc.d/init.d/*
/etc/rc.d/rc0.d/*
/etc/rc.d/rc3.d/*
/etc/rc.d/rc5.d/*

#/etc/init.d/*
#/etc/rc0.d/*
#/etc/rc3.d/*
#/etc/rc5.d/*

#/sbin/rc.d/*
#/sbin/rc.d/rc0.d/*
#/sbin/rc.d/rc3.d/*
#/sbin/rc.d/rc5.d/*

/usr/bin/*
/usr/lib/*.so*
%dir /usr/lib/cups
%dir /usr/lib/cups/backend
/usr/lib/cups/backend/*
%dir /usr/lib/cups/cgi-bin
/usr/lib/cups/cgi-bin/*
%dir /usr/lib/cups/filter
/usr/lib/cups/filter/hpgltops
/usr/lib/cups/filter/imagetops
/usr/lib/cups/filter/imagetoraster
/usr/lib/cups/filter/pdftops
/usr/lib/cups/filter/pstops
/usr/lib/cups/filter/rastertoepson
/usr/lib/cups/filter/rastertohp
/usr/lib/cups/filter/texttops
/usr/man/*
/usr/sbin/*
%dir /usr/share/cups
%dir /usr/share/cups/banners
/usr/share/cups/banners/*
%dir /usr/share/cups/charsets
/usr/share/cups/charsets/*
%dir /usr/share/cups/data
/usr/share/cups/data/*
%dir /usr/share/cups/model
/usr/share/cups/model/*
%dir /usr/share/cups/templates
/usr/share/cups/templates/*
%dir /usr/share/doc/cups
/usr/share/doc/cups/*
%dir /usr/share/locale
/usr/share/locale/*
%attr(0700,lp,root) %dir /var/spool/cups
%attr(1700,lp,root) %dir /var/spool/cups/tmp

%files devel
%dir /usr/include/cups
/usr/include/cups/*
/usr/lib/*.a

%files pstoraster
%dir /usr/lib/cups/filter
/usr/lib/cups/filter/pstoraster
%dir /usr/share/cups/fonts
/usr/share/cups/fonts/*
%dir /usr/share/cups/pstoraster
/usr/share/cups/pstoraster/*

#
# End of "$Id: cups.spec,v 1.30.2.1 2001/05/13 18:37:58 mike Exp $".
#
