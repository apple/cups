#
# "$Id: ipp.spec,v 1.1.2.1 2002/03/22 16:01:37 mike Exp $"
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

Summary: Common Unix Printing System IPP Library
Name: ipp
Version: 1.2.0a1
Release: 0
Copyright: GPL
Group: System Environment/Daemons
Source: ftp://ftp.easysw.com/pub/cups/%{version}/cups-%{version}-source.tar.gz
Url: http://www.cups.org
Packager: Michael Sweet <mike@easysw.com>
Vendor: Easy Software Products

# Use buildroot so as not to disturb the version already installed
BuildRoot: /var/tmp/%{name}-root

%package devel
Summary: Common Unix Printing System IPP Library - development environment
Group: Development/Libraries

%description
The IPP library provides Internet Printing Protocol client and
server functionality.

%description devel
The IPP library provides Internet Printing Protocol client and
server functionality. This is the development package for
creating applications that support IPP.

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

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root)
/usr/lib/*.so*

%files devel
# /usr/bin/ipp-config
%dir /usr/include/ipp
/usr/include/ipp/*
/usr/lib/*.a
#%dir /usr/share/man/cat3
#/usr/share/man/cat3/*
#%dir /usr/share/man/man3
#/usr/share/man/man3/*

#
# End of "$Id: ipp.spec,v 1.1.2.1 2002/03/22 16:01:37 mike Exp $".
#
