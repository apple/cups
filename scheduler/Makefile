#
# "$Id: Makefile,v 1.36.2.2 2002/01/02 18:04:57 mike Exp $"
#
#   Scheduler Makefile for the Common UNIX Printing System (CUPS).
#
#   Copyright 1997-2002 by Easy Software Products, all rights reserved.
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

include ../Makedefs

CUPSDOBJS =	auth.o banners.o cert.o classes.o client.o conf.o devices.o \
		dirsvc.o main.o ipp.o listen.o job.o log.o ppds.o printers.o \
		quotas.o server.o
MIMEOBJS =	filter.o mime.o type.o
OBJS	=	$(CUPSDOBJS) $(MIMEOBJS) cups-lpd.o cups-polld.o testmime.o \
		testspeed.o


#
# Make everything...
#

all:	cupsd cups-lpd cups-polld libmime.a


#
# Clean all object files...
#

clean:
	$(RM) $(OBJS) cupsd cups-lpd cups-polld libmime.a testmime testspeed


#
# Install the scheduler...
#

install:
	$(INSTALL_DIR) $(SBINDIR)
	$(INSTALL_BIN) cupsd $(SBINDIR)
	$(INSTALL_DIR) $(SERVERBIN)/daemon
	$(INSTALL_BIN) cups-lpd $(SERVERBIN)/daemon
	$(INSTALL_BIN) cups-polld $(SERVERBIN)/daemon
	$(INSTALL_DIR) -m 711 -o $(CUPS_USER) -g $(CUPS_GROUP) $(SERVERROOT)/certs
	$(INSTALL_DIR) $(SERVERROOT)/interfaces
	$(INSTALL_DIR) $(SERVERROOT)/ppd
	$(INSTALL_DIR) $(LOGDIR)
	$(INSTALL_DIR) -m 700 -o $(CUPS_USER) -g $(CUPS_GROUP) $(REQUESTS)
	$(INSTALL_DIR) -m 1700 -o $(CUPS_USER) -g $(CUPS_GROUP) $(REQUESTS)/tmp


#
# Make the scheduler executable, "cupsd".
#

cupsd:	$(CUPSDOBJS) libmime.a ../cups/$(LIBCUPS)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o cupsd $(CUPSDOBJS) libmime.a \
		$(LIBZ) $(SSLLIBS) $(LIBSLP) $(PAMLIBS) $(LIBS) $(LIBMALLOC)

$(CUPSDOBJS):	auth.h banners.h cert.h classes.h client.h conf.h \
		cupsd.h dirsvc.h job.h mime.h printers.h \
		../cups/cups.h ../cups/http.h ../cups/ipp.h \
		../cups/language.h ../cups/string.h


#
# Make the line printer daemon, "cups-lpd".
#

cups-lpd:	cups-lpd.o ../cups/$(LIBCUPS)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o cups-lpd cups-lpd.o $(LIBS)

cups-lpd.o:	../cups/cups.h ../cups/http.h ../cups/ipp.h \
		../cups/language.h ../cups/string.h


#
# Make the polling daemon, "cups-polld".
#

cups-polld:	cups-polld.o ../cups/$(LIBCUPS)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o cups-polld cups-polld.o $(LIBS)

cups-polld.o:	../cups/cups.h ../cups/http.h ../cups/ipp.h \
		../cups/language.h ../cups/string.h


#
# libmime.a
#

libmime.a:	$(MIMEOBJS)
	echo Archiving $@...
	$(RM) $@
	$(AR) $(ARFLAGS) $@ $(MIMEOBJS)
	$(RANLIB) $@

$(MIMEOBJS):	mime.h


#
# testmime
#

testmime:	testmime.o libmime.a
	echo Linking $@...
	$(CC) $(LDFLAGS) -o $@ testmime.o libmime.a ../cups/libcups.a

testmime.o:	mime.h

#
# Make the test program, "testspeed".
#

testspeed:	testspeed.o ../cups/$(LIBCUPS)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o testspeed testspeed.o $(LIBS)

testspeed.o:	../cups/cups.h ../cups/http.h ../cups/ipp.h ../cups/language.h

$(OBJS):	../config.h ../Makedefs


#
# End of "$Id: Makefile,v 1.36.2.2 2002/01/02 18:04:57 mike Exp $".
#
