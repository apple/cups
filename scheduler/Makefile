#
# "$Id$"
#
#   Scheduler Makefile for the Common UNIX Printing System (CUPS).
#
#   Copyright 1997-2006 by Easy Software Products, all rights reserved.
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
#       Hollywood, Maryland 20636 USA
#
#       Voice: (301) 373-9600
#       EMail: cups-info@cups.org
#         WWW: http://www.cups.org
#

include ../Makedefs

CUPSDOBJS =	\
		auth.o \
		banners.o \
		cert.o \
		classes.o \
		client.o \
		conf.o \
		dirsvc.o \
		env.o \
		main.o \
		ipp.o \
		listen.o \
		job.o \
		log.o \
		network.o \
		policy.o \
		printers.o \
		process.o \
		quotas.o \
		server.o \
		statbuf.o \
		subscriptions.o \
		sysman.o
MIMEOBJS =	\
		filter.o \
		mime.o \
		type.o
OBJS	=	\
		$(CUPSDOBJS) \
		$(MIMEOBJS) \
		cups-deviced.o \
		cups-driverd.o \
		cups-lpd.o \
		cups-polld.o \
		testdirsvc.o \
		testmime.o \
		testspeed.o \
		util.o
TARGETS	=	\
		cupsd \
		cups-deviced \
		cups-driverd \
		cups-lpd \
		cups-polld \
		libmime.a \
		testdirsvc \
		testmime \
		testspeed


#
# Make everything...
#

all:		$(TARGETS)


#
# Clean all object files...
#

clean:
	$(RM) $(OBJS)
	$(RM) $(TARGETS)


#
# Update dependencies (without system header dependencies...)
#

depend:
	makedepend -Y -I.. -fDependencies $(OBJS:.o=.c) >/dev/null 2>&1


#
# Install the scheduler...
#

install:	all
	echo Creating $(SBINDIR)...
	$(INSTALL_DIR) -m 755 $(SBINDIR)
	echo Installing cupsd in $(SBINDIR)...
	$(INSTALL_BIN) cupsd $(SBINDIR)
	echo Creating $(SERVERBIN)...
	$(INSTALL_DIR) -m 755 $(SERVERBIN)
	echo Creating $(SERVERBIN)/daemon...
	$(INSTALL_DIR) -m 755 $(SERVERBIN)/daemon
	echo Installing cups-deviced, cups-driverd, cups-lpd, and cups-polld in $(SERVERBIN)/daemon...
	$(INSTALL_BIN) cups-deviced $(SERVERBIN)/daemon
	$(INSTALL_BIN) cups-driverd $(SERVERBIN)/daemon
	$(INSTALL_BIN) cups-lpd $(SERVERBIN)/daemon
	$(INSTALL_BIN) cups-polld $(SERVERBIN)/daemon
	echo Creating $(SERVERBIN)/driver...
	$(INSTALL_DIR) -m 755 $(SERVERBIN)/driver
	echo Creating $(SERVERROOT)...
	$(INSTALL_DIR) -m 755 $(SERVERROOT)
	-chgrp $(CUPS_GROUP) $(SERVERROOT)
	echo Creating $(SERVERROOT)/interfaces...
	$(INSTALL_DIR) -m 755 $(SERVERROOT)/interfaces
	-chgrp $(CUPS_GROUP) $(SERVERROOT)/interfaces
	echo Creating $(SERVERROOT)/ppd...
	$(INSTALL_DIR) -m 755 $(SERVERROOT)/ppd
	-chgrp $(CUPS_GROUP) $(SERVERROOT)/ppd
	echo Creating $(STATEDIR)...
	$(INSTALL_DIR) -m 755 $(STATEDIR)
	echo Creating $(STATEDIR)/certs...
	$(INSTALL_DIR) -m 511 $(STATEDIR)/certs
	-chgrp $(CUPS_PRIMARY_SYSTEM_GROUP) $(STATEDIR)/certs
	echo Creating $(LOGDIR)...
	$(INSTALL_DIR) -m 755 $(LOGDIR)
	echo Creating $(REQUESTS)...
	$(INSTALL_DIR) -m 710 $(REQUESTS)
	-chgrp $(CUPS_GROUP) $(REQUESTS)
	echo Creating $(REQUESTS)/tmp...
	$(INSTALL_DIR) -m 1770 $(REQUESTS)/tmp
	-chgrp $(CUPS_GROUP) $(REQUESTS)/tmp
	echo Creating $(CACHEDIR)...
	$(INSTALL_DIR) -m 775 $(CACHEDIR)
	-chgrp $(CUPS_GROUP) $(CACHEDIR)
#	echo Creating $(CACHEDIR)/ppd...
#	$(INSTALL_DIR) -m 755 $(CACHEDIR)/ppd
#	-chgrp $(CUPS_GROUP) $(CACHEDIR)/ppd


#
# Uninstall the scheduler...
#

uninstall:	all
	$(RM) $(SBINDIR)/cupsd
	$(RM) $(SERVERBIN)/daemon/cups-deviced
	$(RM) $(SERVERBIN)/daemon/cups-driverd
	$(RM) $(SERVERBIN)/daemon/cups-lpd
	$(RM) $(SERVERBIN)/daemon/cups-polld
	-$(RMDIR) $(STATEDIR)/certs
	-$(RMDIR) $(STATEDIR)
	-$(RMDIR) $(SERVERROOT)/ppd
	-$(RMDIR) $(SERVERROOT)/interfaces
	-$(RMDIR) $(SERVERROOT)
	-$(RMDIR) $(SERVERBIN)/driver
	-$(RMDIR) $(SERVERBIN)/daemon
	-$(RMDIR) $(SERVERBIN)
	-$(RMDIR) $(SBINDIR)
	-$(RMDIR) $(REQUESTS)/tmp
	-$(RMDIR) $(REQUESTS)
	-$(RMDIR) $(LOGDIR)
	-$(RMDIR) $(CACHEDIR)


#
# Make the scheduler executable, "cupsd".
#

cupsd:	$(CUPSDOBJS) libmime.a ../cups/$(LIBCUPS)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o cupsd $(CUPSDOBJS) libmime.a \
		$(LIBZ) $(SSLLIBS) $(LIBSLP) $(LIBLDAP) $(PAMLIBS) \
		$(LIBPAPER) $(LIBMALLOC) $(CUPSDLIBS) $(LIBS)

cupsd-static:	$(CUPSDOBJS) libmime.a ../cups/libcups.a
	echo Linking $@...
	$(CC) $(LDFLAGS) -o cupsd-static $(CUPSDOBJS) libmime.a \
		$(LIBZ) $(SSLLIBS) $(LIBSLP) $(LIBLDAP) $(PAMLIBS) \
		../cups/libcups.a $(COMMONLIBS) $(LIBZ) $(LIBPAPER) \
		$(LIBMALLOC) $(CUPSDLIBS)


#
# Make the device daemon, "cups-deviced".
#

cups-deviced:	cups-deviced.o util.o ../cups/$(LIBCUPS)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o cups-deviced cups-deviced.o util.o $(LIBS)


#
# Make the driver daemon, "cups-driverd".
#

cups-driverd:	cups-driverd.o util.o ../cups/$(LIBCUPS)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o cups-driverd cups-driverd.o util.o $(LIBS)


#
# Make the line printer daemon, "cups-lpd".
#

cups-lpd:	cups-lpd.o ../cups/$(LIBCUPS)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o cups-lpd cups-lpd.o $(LIBS)


#
# Make the polling daemon, "cups-polld".
#

cups-polld:	cups-polld.o ../cups/$(LIBCUPS)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o cups-polld cups-polld.o $(LIBS)


#
# libmime.a
#

libmime.a:	$(MIMEOBJS)
	echo Archiving $@...
	$(RM) $@
	$(AR) $(ARFLAGS) $@ $(MIMEOBJS)
	$(RANLIB) $@


#
# Make the test program, "testdirsvc".
#

testdirsvc:	testdirsvc.o
	echo Linking $@...
	$(CC) $(LDFLAGS) -o testdirsvc testdirsvc.o $(COMMONLIBS) $(NETLIBS)


#
# testmime
#

testmime:	testmime.o libmime.a ../cups/libcups.a
	echo Linking $@...
	$(CC) $(LDFLAGS) -o $@ testmime.o libmime.a ../cups/libcups.a \
		$(COMMONLIBS) $(LIBZ) $(SSLLIBS)


#
# Make the test program, "testspeed".
#

testspeed:	testspeed.o ../cups/$(LIBCUPS)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o testspeed testspeed.o $(LIBS)


#
# Dependencies...
#
  
include Dependencies


#
# End of "$Id$".
#
