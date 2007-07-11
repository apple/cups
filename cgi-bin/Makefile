#
# "$Id$"
#
#   CGI makefile for the Common UNIX Printing System (CUPS).
#
#   Copyright 2007 by Apple Inc.
#   Copyright 1997-2006 by Easy Software Products.
#
#   These coded instructions, statements, and computer programs are the
#   property of Apple Inc. and are protected by Federal copyright
#   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
#   which should have been included with this file.  If this file is
#   file is missing or damaged, see the license at "http://www.cups.org/".
#

include ../Makedefs

CGIS	=	admin.cgi classes.cgi help.cgi jobs.cgi printers.cgi
TARGETS	=	libcgi.a $(CGIS) testcgi testhi testtemplate
LIBOBJS	=	help-index.o html.o ipp-var.o search.o template.o var.o
OBJS	=	$(LIBOBJS) admin.o classes.o help.o \
		jobs.o printers.o testcgi.o testhi.o testtemplate.o


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
	$(INSTALL_DIR) -m 755 $(SERVERBIN)/cgi-bin
	for file in $(CGIS); do \
		$(INSTALL_BIN) $$file $(SERVERBIN)/cgi-bin; \
	done
	if test "x$(SYMROOT)" != "x"; then \
		$(INSTALL_DIR) $(SYMROOT); \
		for file in $(TARGETS); do \
			cp $$file $(SYMROOT); \
		done \
	fi


#
# Uninstall all targets...
#

uninstall:
	for file in $(CGIS); do \
		$(RM) $(SERVERBIN)/cgi-bin/$$file; \
	done
	-$(RMDIR) $(SERVERBIN)/cgi-bin


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

testcgi:	testcgi.o ../Makedefs libcgi.a ../cups/libcups.a
	echo Linking $@...
	$(CC) $(LDFLAGS) -o $@ testcgi.o libcgi.a ../cups/libcups.a \
		$(COMMONLIBS) $(SSLLIBS) $(LIBZ) $(LIBGSSAPI)


#
# testhi
#

testhi:	testhi.o ../Makedefs libcgi.a ../cups/libcups.a
	echo Linking $@...
	$(CC) $(LDFLAGS) -o $@ testhi.o libcgi.a ../cups/libcups.a \
		$(COMMONLIBS) $(SSLLIBS) $(LIBZ) $(LIBGSSAPI)


#
# testtemplate
#

testtemplate:	testtemplate.o ../Makedefs libcgi.a ../cups/libcups.a
	echo Linking $@...
	$(CC) $(LDFLAGS) -o $@ testtemplate.o libcgi.a ../cups/libcups.a \
		$(COMMONLIBS) $(SSLLIBS) $(LIBZ) $(LIBGSSAPI)


#
# Dependencies...
#

include Dependencies


#
# End of "$Id$".
#
