#
# "$Id: Makefile,v 1.25 2000/03/30 05:19:28 mike Exp $"
#
#   Scheduler Makefile for the Common UNIX Printing System (CUPS).
#
#   Copyright 1997-2000 by Easy Software Products, all rights reserved.
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
		dirsvc.o main.o ipp.o listen.o job.o log.o ppds.o printers.o
MIMEOBJS =	filter.o mime.o type.o
OBJS	=	$(CUPSDOBJS) $(MIMEOBJS) cups-polld.o testmime.o testspeed.o


#
# Make everything...
#

all:	cupsd cups-polld libmime.a testmime testspeed


#
# Clean all object files...
#

clean:
	$(RM) $(OBJS) cupsd cups-polld libmime.a testmime testspeed


#
# Install the scheduler...
#

install:
	-$(MKDIR) $(SBINDIR)
	$(INSTALL_BIN) cupsd $(SBINDIR)
	$(CHMOD) g-rwx,o-rwx $(SBINDIR)/cupsd
	-$(MKDIR) $(SERVERBIN)/daemon
	$(INSTALL_BIN) cups-polld $(SERVERBIN)/daemon
	-$(MKDIR) $(SERVERROOT)/certs
	-$(MKDIR) $(SERVERROOT)/interfaces
	-$(MKDIR) $(SERVERROOT)/ppd
	-$(MKDIR) $(LOGDIR)
	-$(MKDIR) $(REQUESTS)


#
# Make the scheduler executable, "cupsd".
#

cupsd:	$(CUPSDOBJS) libmime.a ../cups/$(LIBCUPS)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o cupsd $(CUPSDOBJS) libmime.a $(LIBZ) $(LIBS)

$(CUPSDOBJS):	auth.h banners.h cert.h classes.h client.h conf.h \
		cupsd.h dirsvc.h job.h mime.h printers.h \
		../cups/cups.h ../cups/http.h ../cups/ipp.h \
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
	$(CC) $(LDFLAGS) -o $@ testmime.o libmime.a

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
# End of "$Id: Makefile,v 1.25 2000/03/30 05:19:28 mike Exp $".
#
