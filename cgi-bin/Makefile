#
# "$Id$"
#
#   CGI makefile for the Common UNIX Printing System (CUPS).
#
#   Copyright 1997-2005 by Easy Software Products.
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

CGIS	=	admin.cgi classes.cgi help.cgi jobs.cgi printers.cgi
TARGETS	=	libcgi.a $(CGIS) testcgi testhi
LIBOBJS	=	help-index.o html.o ipp-var.o search.o template.o var.o
OBJS	=	$(LIBOBJS) admin.o classes.o help.o \
		jobs.o printers.o testcgi.o testhi.o


#
# Make all targets...
#

all:	$(TARGETS)


#
# Clean all object files...
#

clean:
	$(RM) $(OBJS) $(TARGETS)


#
# Update dependencies (without system header dependencies...)
#

depend:
	makedepend -Y -I.. -fDependencies $(OBJS:.o=.c) >/dev/null 2>&1


#
# Install all targets...
#

install:	all
	$(INSTALL_DIR) $(SERVERBIN)/cgi-bin
	for file in $(CGIS); do \
		$(INSTALL_BIN) $$file $(SERVERBIN)/cgi-bin; \
	done


#
# libcgi.a
#

libcgi.a:	$(LIBOBJS)
	echo Archiving $@...
	$(RM) $@
	$(AR) $(ARFLAGS) $@ $(LIBOBJS)
	$(RANLIB) $@


#
# admin.cgi
#

admin.cgi:	admin.o ../Makedefs ../cups/$(LIBCUPS) libcgi.a
	echo Linking $@...
	$(CC) $(LDFLAGS) -o $@ admin.o libcgi.a $(LIBS)


#
# classes.cgi
#

classes.cgi:	classes.o ../Makedefs ../cups/$(LIBCUPS) libcgi.a
	echo Linking $@...
	$(CC) $(LDFLAGS) -o $@ classes.o libcgi.a $(LIBS)


#
# help.cgi
#

help.cgi:	help.o ../Makedefs libcgi.a
	echo Linking $@...
	$(CC) $(LDFLAGS) -o $@ help.o libcgi.a $(LIBS)


#
# jobs.cgi
#

jobs.cgi:	jobs.o ../Makedefs ../cups/$(LIBCUPS) libcgi.a
	echo Linking $@...
	$(CC) $(LDFLAGS) -o $@ jobs.o libcgi.a $(LIBS)


#
# printers.cgi
#

printers.cgi:	printers.o ../Makedefs ../cups/$(LIBCUPS) libcgi.a
	echo Linking $@...
	$(CC) $(LDFLAGS) -o $@ printers.o libcgi.a $(LIBS)


#
# testcgi
#

testcgi:	testcgi.o ../Makedefs libcgi.a
	echo Linking $@...
	$(CC) $(LDFLAGS) -o $@ testcgi.o libcgi.a $(LIBS)


#
# testhi
#

testhi:	testhi.o ../Makedefs libcgi.a
	echo Linking $@...
	$(CC) $(LDFLAGS) -o $@ testhi.o libcgi.a $(LIBS)


#
# Dependencies...
#

include Dependencies


#
# End of "$Id$".
#
