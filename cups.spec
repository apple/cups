#
# "$Id: cups.spec,v 1.15 2000/04/09 23:08:57 mike Exp $"
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
Version: 1.1b3
Release: 0
Copyright: GPL
Group: System Environment/Daemons
Source: ftp://ftp.easysw.com/pub/cups/beta/cups-1.1b3-source.tar.gz
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

make prefix=$RPM_BUILD_ROOT/usr LOGDIR=$RPM_BUILD_ROOT/var/log/cups \
	REQUESTS=$RPM_BUILD_ROOT/var/spool/cups \
	SERVERROOT=$RPM_BUILD_ROOT/etc/cups install

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
/etc/rc.d/init.d/cups
%config /etc/cups/*
%dir /etc/cups/certs
%dir /etc/cups/interfaces
%dir /etc/cups/ppd
/usr/bin/*
%%attr(4555,root,root) /usr/bin/lppasswd
/usr/lib/*.so*
/usr/man/*
/usr/sbin/*
%dir /usr/share/cups
/usr/share/cups/*
%dir /usr/share/doc/cups
/usr/share/doc/cups/*
%dir /usr/share/locale
%dir /usr/share/locale/C
%dir /usr/share/locale/de
%dir /usr/share/locale/en
%dir /usr/share/locale/es
%dir /usr/share/locale/fr
%dir /usr/share/locale/it
/usr/share/locale/C/*
/usr/share/locale/de/*
/usr/share/locale/en/*
/usr/share/locale/es/*
/usr/share/locale/fr/*
/usr/share/locale/it/*
%dir /usr/lib/cups
%dir /usr/lib/cups/backend
%dir /usr/lib/cups/cgi-bin
%dir /usr/lib/cups/daemon
%dir /usr/lib/cups/filter
/usr/lib/cups/backend/*
/usr/lib/cups/cgi-bin/*
/usr/lib/cups/daemon/*
/usr/lib/cups/filter/*
%dir /var/spool/cups
%dir /var/log/cups

%files devel
%dir /usr/include/cups
/usr/include/cups/*
%dir /usr/lib
/usr/lib/*.a

#
# End of "$Id: cups.spec,v 1.15 2000/04/09 23:08:57 mike Exp $".
#
