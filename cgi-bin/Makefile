#
# CGI makefile for CUPS.
#
# Copyright © 2007-2019 by Apple Inc.
# Copyright © 1997-2006 by Easy Software Products.
#
# Licensed under Apache License v2.0.  See the file "LICENSE" for more
# information.
#

include ../Makedefs

LIBOBJS	=	\
		help-index.o \
		html.o \
		ipp-var.o \
		search.o \
		template.o \
		var.o
OBJS	=	\
		$(LIBOBJS) \
		admin.o \
		classes.o \
		help.o \
		jobs.o \
		makedocset.o \
		printers.o \
		testcgi.o \
		testhi.o \
		testtemplate.o
CGIS	=	\
		admin.cgi \
		classes.cgi \
		help.cgi \
		jobs.cgi \
		printers.cgi
LIBTARGETS =	\
		libcupscgi.a

UNITTARGETS =	\
		testcgi \
		testhi \
		testtemplate

TARGETS	=	\
		$(LIBTARGETS) \
		$(CGIS)


#
# Make all targets...
#

all:	$(TARGETS)


#
# Make library targets...
#

libs:


#
# Make unit tests...
#

unittests:	$(UNITTARGETS)


#
# Clean all object files...
#

clean:
	$(RM) $(OBJS) $(TARGETS) $(UNITTARGETS) makedocset


#
# Update dependencies (without system header dependencies...)
#

depend:
	$(CC) -MM $(ALL_CFLAGS) $(OBJS:.o=.c) >Dependencies


#
# Install all targets...
#

install:	all install-data install-headers install-libs install-exec


#
# Install data files...
#

install-data:


#
# Install programs...
#

install-exec:
	$(INSTALL_DIR) -m 755 $(SERVERBIN)/cgi-bin
	for file in $(CGIS); do \
		$(INSTALL_BIN) $$file $(SERVERBIN)/cgi-bin; \
	done
	if test "x$(SYMROOT)" != "x"; then \
		$(INSTALL_DIR) $(SYMROOT); \
		for file in $(CGIS); do \
			cp $$file $(SYMROOT); \
			dsymutil $(SYMROOT)/$$file; \
		done \
	fi


#
# Install headers...
#

install-headers:


#
# Install libraries...
#

install-libs:


#
# Uninstall all targets...
#

uninstall:
	for file in $(CGIS); do \
		$(RM) $(SERVERBIN)/cgi-bin/$$file; \
	done
	-$(RMDIR) $(SERVERBIN)/cgi-bin


#
# libcupscgi.a
#

libcupscgi.a:	$(LIBOBJS)
	echo Archiving $@...
	$(RM) $@
	$(AR) $(ARFLAGS) $@ $(LIBOBJS)
	$(RANLIB) $@


#
# admin.cgi
#

admin.cgi:	admin.o ../Makedefs ../cups/$(LIBCUPS) libcupscgi.a
	echo Linking $@...
	$(LD_CC) $(ALL_LDFLAGS) -o $@ admin.o libcupscgi.a $(LINKCUPS)
	$(CODE_SIGN) -s "$(CODE_SIGN_IDENTITY)" $@


#
# classes.cgi
#

classes.cgi:	classes.o ../Makedefs ../cups/$(LIBCUPS) libcupscgi.a
	echo Linking $@...
	$(LD_CC) $(ALL_LDFLAGS) -o $@ classes.o libcupscgi.a $(LINKCUPS)
	$(CODE_SIGN) -s "$(CODE_SIGN_IDENTITY)" $@


#
# help.cgi
#

help.cgi:	help.o ../Makedefs ../cups/$(LIBCUPS) libcupscgi.a
	echo Linking $@...
	$(LD_CC) $(ALL_LDFLAGS) -o $@ help.o libcupscgi.a $(LINKCUPS)
	$(CODE_SIGN) -s "$(CODE_SIGN_IDENTITY)" $@


#
# jobs.cgi
#

jobs.cgi:	jobs.o ../Makedefs ../cups/$(LIBCUPS) libcupscgi.a
	echo Linking $@...
	$(LD_CC) $(ALL_LDFLAGS) -o $@ jobs.o libcupscgi.a $(LINKCUPS)
	$(CODE_SIGN) -s "$(CODE_SIGN_IDENTITY)" $@


#
# printers.cgi
#

printers.cgi:	printers.o ../Makedefs ../cups/$(LIBCUPS) libcupscgi.a
	echo Linking $@...
	$(LD_CC) $(ALL_LDFLAGS) -o $@ printers.o libcupscgi.a $(LINKCUPS)
	$(CODE_SIGN) -s "$(CODE_SIGN_IDENTITY)" $@


#
# testcgi
#

testcgi:	testcgi.o ../Makedefs libcupscgi.a ../cups/$(LIBCUPSSTATIC)
	echo Linking $@...
	$(LD_CC) $(ARCHFLAGS) $(ALL_LDFLAGS) -o $@ testcgi.o libcupscgi.a \
		$(LINKCUPSSTATIC)
	$(CODE_SIGN) -s "$(CODE_SIGN_IDENTITY)" $@
	echo Testing CGI API...
	./testcgi


#
# testhi
#

testhi:	testhi.o ../Makedefs libcupscgi.a ../cups/$(LIBCUPSSTATIC)
	echo Linking $@...
	$(LD_CC) $(ARCHFLAGS) $(ALL_LDFLAGS) -o $@ testhi.o libcupscgi.a \
		$(LINKCUPSSTATIC)
	$(CODE_SIGN) -s "$(CODE_SIGN_IDENTITY)" $@
	echo Testing help index API...
	./testhi


#
# testtemplate
#

testtemplate:	testtemplate.o ../Makedefs libcupscgi.a ../cups/$(LIBCUPSSTATIC)
	echo Linking $@...
	$(LD_CC) $(ALL_LDFLAGS) -o $@ testtemplate.o libcupscgi.a $(LINKCUPSSTATIC)
	$(CODE_SIGN) -s "$(CODE_SIGN_IDENTITY)" $@


#
# Dependencies...
#

include Dependencies
