#
# "$Id: cups.spec,v 1.19 2000/07/10 20:58:19 mike Exp $"
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
Version: 1.1
Release: 0
Copyright: GPL
Group: System Environment/Daemons
Source: ftp://ftp.easysw.com/pub/cups/1.1/cups-1.1-source.tar.gz
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

make	datadir=$RPM_BUILD_ROOT/usr/share \
	exec_prefix=$RPM_BUILD_ROOT/usr \
	includedir=$RPM_BUILD_ROOT/usr/include \
	infodir=$RPM_BUILD_ROOT/usr/info \
	libdir=$RPM_BUILD_ROOT/usr/lib \
	localestatedir=$RPM_BUILD_ROOT/var \
	prefix=$RPM_BUILD_ROOT \
	sharedstatedir=$RPM_BUILD_ROOT/usr/com \
	sysconfdir=$RPM_BUILD_ROOT/etc \
	install

install -m 755 -o root -g root cups.sh $RPM_BUILD_ROOT/etc/rc.d/init.d/cups

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
/etc/*
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
# End of "$Id: cups.spec,v 1.19 2000/07/10 20:58:19 mike Exp $".
#
