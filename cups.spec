#
# "$Id: cups.spec,v 1.10 1999/11/04 13:35:01 mike Exp $"
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
Version: 1.0.2
Release: 0
Copyright: GPL
Group: System Environment/Daemons
Source: ftp://ftp.easysw.com/pub/cups/1.0.2/cups-1.0.2-source.tar.gz
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
mkdir -p $RPM_BUILD_ROOT/var/log

ln -sf /var/cups/logs $RPM_BUILD_ROOT/var/log/cups
ln -sf /var/cups/conf $RPM_BUILD_ROOT/etc/cups

make prefix=$RPM_BUILD_ROOT/usr DATADIR=$RPM_BUILD_ROOT/usr/share/cups LOCALEDIR=$RPM_BUILD_ROOT/usr/share/locale SERVERROOT=$RPM_BUILD_ROOT/var/cups install

install -m 755 -o root -g root cups.sh $RPM_BUILD_ROOT/etc/rc.d/init.d/cups

ln -sf /usr/sbin/accept $RPM_BUILD_ROOT/usr/bin/disable
ln -sf /usr/sbin/accept $RPM_BUILD_ROOT/usr/bin/enable
ln -sf /usr/sbin/accept $RPM_BUILD_ROOT/usr/lib/accept
ln -sf /usr/sbin/accept $RPM_BUILD_ROOT/usr/lib/reject
ln -sf /usr/sbin/accept $RPM_BUILD_ROOT/usr/sbin/reject
ln -sf /usr/sbin/lpadmin $RPM_BUILD_ROOT/usr/lib/lpadmin

%post
/sbin/chkconfig --add cups

%preun
/sbin/chkconfig --del cups

%clean
rm -rf $RPM_BUILD_ROOT

%files
/etc/rc.d/init.d/cups
%config /var/cups/conf/*
/usr/bin/*
/usr/lib/*
/usr/man/*
/usr/sbin/*
%dir /usr/share/cups
/usr/share/cups/*
%dir /var/cups
/var/cups/backend/*
/var/cups/cgi-bin/*
/var/cups/filter/*
%dir /var/cups/interfaces
%dir /var/cups/logs
%dir /var/cups/ppd
%dir /var/cups/requests
%dir /etc/cups
%dir /var/log/cups

%files devel
%dir /usr/include/cups
/usr/include/cups/*

#
# End of "$Id: cups.spec,v 1.10 1999/11/04 13:35:01 mike Exp $".
#
