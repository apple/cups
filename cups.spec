#
# "$Id: cups.spec,v 1.30.2.9 2002/05/21 20:05:48 mike Exp $"
#
#   RPM "spec" file for the Common UNIX Printing System (CUPS).
#
#   Original version by Jason McMullan <jmcc@ontv.com>.
#
#   Copyright 1999-2002 by Easy Software Products, all rights reserved.
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
Version: 1.2.0a1
Release: 0
Copyright: GPL
Group: System Environment/Daemons
Source: ftp://ftp.easysw.com/pub/cups/%{version}/cups-%{version}-source.tar.gz
Url: http://www.cups.org
Packager: Anonymous <anonymous@foo.com>
Vendor: Easy Software Products

# Use buildroot so as not to disturb the version already installed
BuildRoot: /var/tmp/%{name}-root

%package devel
Summary: Common Unix Printing System - development environment
Group: Development/Libraries
Provides: libcups1

%description
The Common UNIX Printing System provides a portable printing layer for
UNIX® operating systems.  It has been developed by Easy Software Products
to promote a standard printing solution for all UNIX vendors and users.
CUPS provides the System V and Berkeley command-line interfaces.

%description devel
The Common UNIX Printing System provides a portable printing layer for 
UNIX® operating systems.  This is the development package for creating
additional printer drivers and other CUPS services.

%prep
%setup

%build
CFLAGS="$RPM_OPT_FLAGS" CXXFLAGS="$RPM_OPT_FLAGS" LDFLAGS="$RPM_OPT_FLAGS" ./configure

# If we got this far, all prerequisite libraries must be here.
make

%install
# Make sure the RPM_BUILD_ROOT directory exists.
rm -rf $RPM_BUILD_ROOT

make BUILDROOT=$RPM_BUILD_ROOT install

%post
ldconfig

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

/usr/bin/cancel
/usr/bin/disable
/usr/bin/enable
/usr/bin/lp*
/usr/lib/*.so*
%dir /usr/lib/cups
/usr/lib/cups/*
/usr/sbin/*
%dir /usr/share/cups
/usr/share/cups/*
%dir /usr/share/doc/cups
/usr/share/doc/cups/*
%dir /usr/share/locale
/usr/share/locale/*

%dir /usr/share/man/cat1
/usr/share/man/cat1/*
%dir /usr/share/man/cat5
/usr/share/man/cat5/*
%dir /usr/share/man/cat8
/usr/share/man/cat8/*
%dir /usr/share/man/man1
/usr/share/man/man1/*
%dir /usr/share/man/man5
/usr/share/man/man5/*
%dir /usr/share/man/man8
/usr/share/man/man8/*

%dir /usr/share/man/fr/cat1
/usr/share/man/fr/cat1/*
%dir /usr/share/man/fr/cat5
/usr/share/man/fr/cat5/*
%dir /usr/share/man/fr/cat8
/usr/share/man/fr/cat8/*
%dir /usr/share/man/fr/man1
/usr/share/man/fr/man1/*
%dir /usr/share/man/fr/man5
/usr/share/man/fr/man5/*
%dir /usr/share/man/fr/man8
/usr/share/man/fr/man8/*

%attr(0700,lp,root) %dir /var/spool/cups
%attr(1700,lp,root) %dir /var/spool/cups/tmp

%files devel
/usr/bin/cups-config
%dir /usr/include/cups
/usr/include/cups/*
/usr/lib/*.a

%dir /usr/share/man/cat3
/usr/share/man/cat3/*
%dir /usr/share/man/man3
/usr/share/man/man3/*

%dir /usr/share/man/fr/cat3
/usr/share/man/fr/cat3/*
%dir /usr/share/man/fr/man3
/usr/share/man/fr/man3/*

#
# End of "$Id: cups.spec,v 1.30.2.9 2002/05/21 20:05:48 mike Exp $".
#
