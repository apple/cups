#
# "$Id: cups.spec,v 1.22 2000/08/01 18:18:02 mike Exp $"
#
#   RPM "spec" file for the Common UNIX Printing System (CUPS).
#
#   Original version by Jason McMullan <jmcc@ontv.com>.
#
#   Copyright 1999-2000 by Easy Software Products, all rights reserved.
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
Version: 1.1.2
Release: 0
Copyright: GPL
Group: System Environment/Daemons
Source: ftp://ftp.easysw.com/pub/cups/%version/cups-%version-source.tar.gz
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
mkdir -p $RPM_BUILD_ROOT/etc/rc.d/init.d

make	prefix=$RPM_BUILD_ROOT \
	exec_prefix=$RPM_BUILD_ROOT/usr \
	BINDIR=$RPM_BUILD_ROOT/usr/bin \
	DATADIR=$RPM_BUILD_ROOT/usr/share/cups \
	DOCDIR=$RPM_BUILD_ROOT/usr/share/doc/cups \
	INCLUDEDIR=$RPM_BUILD_ROOT/usr/include \
	LIBDIR=$RPM_BUILD_ROOT/usr/lib \
	LOCALEDIR=$RPM_BUILD_ROOT/usr/share/locale \
	MANDIR=$RPM_BUILD_ROOT/usr/man \
	PAMDIR=$RPM_BUILD_ROOT/etc/pam.d \
	REQUESTS=$RPM_BUILD_ROOT/var/spool/cups \
	SBINDIR=$RPM_BUILD_ROOT/usr/sbin \
	SERVERBIN=$RPM_BUILD_ROOT/usr/lib/cups \
	SERVERROOT=$RPM_BUILD_ROOT/etc/cups \
	install

%post
/sbin/chkconfig --add cups
/sbin/chkconfig cups on
/etc/rc.d/init.d/cups start

%preun
/etc/rc.d/init.d/cups stop
/sbin/chkconfig --del cups

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root)
%config /etc/cups/*
/etc/pam.d/*
/etc/rc.d/*
/usr/bin/*
/usr/lib/*.so*
/usr/man/*
/usr/sbin/*
/usr/share/*
/usr/lib/cups/*
/var/*

%files devel
/usr/include/cups/*
/usr/lib/*.a

#
# End of "$Id: cups.spec,v 1.22 2000/08/01 18:18:02 mike Exp $".
#
