#
# "$Id: Makefile,v 1.7 2000/01/30 13:38:16 mike Exp $"
#
#   CGI makefile for the Common UNIX Printing System (CUPS).
#
#   Copyright 1997-1999 by Easy Software Products.
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

TARGETS	=	libcgi.a admin.cgi classes.cgi jobs.cgi printers.cgi
LIBOBJS	=	abort.o email.o html.o template.o var.o
OBJS	=	$(LIBOBJS) admin.o classes.o jobs.o printers.o


#
# Make all targets...
#

all:	$(TARGETS)


#
# Clean all object files...
#

clean:
	rm -f $(OBJS) $(TARGETS)


#
# Install all targets...
#

install:
	-$(MKDIR) $(SERVERROOT)/cgi-bin
	$(CP)  $(SERVERROOT)/cgi-bin


#
# libcgi.a
#

libcgi.a:	$(LIBOBJS)
	echo Archiving $@...
	$(RM) $@
	$(AR) $(ARFLAGS) $@ $(OBJS)
	$(RANLIB) $@

$(LIBOBJS):	cgi.h


#
# admin.cgi
#

admin.cgi:	admin.o ../Makedefs ../cups/$(LIBCUPS) libcgi.a
	echo Linking $@...
	$(CC) $(LDFLAGS) -o $@ admin.o libcgi.a $(LIBS)

admin.o:	cgi.h ../cups/cups.h ../cups/ipp.h ../cups/language.h


#
# classes.cgi
#

classes.cgi:	classes.o ../Makedefs ../cups/$(LIBCUPS) libcgi.a
	echo Linking $@...
	$(CC) $(LDFLAGS) -o $@ classes.o libcgi.a $(LIBS)

classes.o:	cgi.h ../cups/cups.h ../cups/ipp.h ../cups/language.h


#
# jobs.cgi
#

jobs.cgi:	jobs.o ../Makedefs ../cups/$(LIBCUPS) libcgi.a
	echo Linking $@...
	$(CC) $(LDFLAGS) -o $@ jobs.o libcgi.a $(LIBS)

jobs.o:	cgi.h ../cups/cups.h ../cups/ipp.h ../cups/language.h


#
# printers.cgi
#

printers.cgi:	printers.o ../Makedefs ../cups/$(LIBCUPS) libcgi.a
	echo Linking $@...
	$(CC) $(LDFLAGS) -o $@ printers.o libcgi.a $(LIBS)

printers.o:	cgi.h ../cups/cups.h ../cups/ipp.h ../cups/language.h

$(OBJS):	../Makedefs

#
# End of "$Id: Makefile,v 1.7 2000/01/30 13:38:16 mike Exp $".
#
