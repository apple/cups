#
# "$Id: Makefile 7871 2008-08-27 21:12:43Z mike $"
#
#   CGI makefile for CUPS.
#
#   Copyright 2007-2012 by Apple Inc.
#   Copyright 1997-2006 by Easy Software Products.
#
#   These coded instructions, statements, and computer programs are the
#   property of Apple Inc. and are protected by Federal copyright
#   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
#   which should have been included with this file.  If this file is
#   file is missing or damaged, see the license at "http://www.cups.org/".
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
		testtemplate.o \
		websearch.o
CGIS	=	\
		admin.cgi \
		classes.cgi \
		help.cgi \
		jobs.cgi \
		printers.cgi
LIBTARGETS =	\
		libcupscgi.a \
		$(LIBCUPSCGI) \
		websearch

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

libs:		$(LIBTARGETS) $(UNITTESTS)


#
# Make unit tests...
#

unittests:	$(UNITTARGETS)


#
# Clean all object files...
#

clean:
	$(RM) $(OBJS) $(TARGETS) $(UNITTARGETS) makedocset
	$(RM) libcupscgi.so libcupscgi.sl libcupscgi.dylib


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
		done \
	fi


#
# Install headers...
#

install-headers:
	echo Installing header files in $(INCLUDEDIR)/cups...
	$(INSTALL_DIR) -m 755 $(INCLUDEDIR)/cups
	$(INSTALL_DATA) cgi.h $(INCLUDEDIR)/cups
	$(INSTALL_DATA) help-index.h $(INCLUDEDIR)/cups


#
# Install libraries...
#

install-libs: $(INSTALLSTATIC)
	echo Installing libraries in $(LIBDIR)...
	$(INSTALL_DIR) -m 755 $(LIBDIR)
	$(INSTALL_LIB) $(LIBCUPSCGI) $(LIBDIR)
	if test $(LIBCUPSCGI) = "libcupscgi.so.1" -o $(LIBCUPSCGI) = "libcupscgi.sl.1"; then \
		$(RM) $(LIBDIR)/`basename $(LIBCUPSCGI) .1`; \
		$(LN) $(LIBCUPSCGI) $(LIBDIR)/`basename $(LIBCUPSCGI) .1`; \
	fi
	if test $(LIBCUPSCGI) = "libcupscgi.1.dylib"; then \
		$(RM) $(LIBDIR)/libcupscgi.dylib; \
		$(LN) $(LIBCUPSCGI) $(LIBDIR)/libcupscgi.dylib; \
	fi
	if test "x$(SYMROOT)" != "x"; then \
		$(INSTALL_DIR) $(SYMROOT); \
		cp $(LIBCUPSCGI) $(SYMROOT); \
	fi

installstatic:
	$(INSTALL_DIR) -m 755 $(LIBDIR)
	$(INSTALL_LIB) -m 755 libcupscgi.a $(LIBDIR)
	$(RANLIB) $(LIBDIR)/libcupscgi.a
	$(CHMOD) 555 $(LIBDIR)/libcupscgi.a


#
# Uninstall all targets...
#

uninstall:
	for file in $(CGIS); do \
		$(RM) $(SERVERBIN)/cgi-bin/$$file; \
	done
	-$(RMDIR) $(SERVERBIN)/cgi-bin
	$(RM) $(LIBDIR)/libcupscgi.1.dylib
	$(RM) $(LIBDIR)/libcupscgi.a
	$(RM) $(LIBDIR)/libcupscgi.dylib
	$(RM) $(LIBDIR)/libcupscgi_s.a
	$(RM) $(LIBDIR)/libcupscgi.sl
	$(RM) $(LIBDIR)/libcupscgi.sl.1
	$(RM) $(LIBDIR)/libcupscgi.so
	$(RM) $(LIBDIR)/libcupscgi.so.1
	-$(RMDIR) $(LIBDIR)
	$(RM) $(INCLUDEDIR)/cups/cgi.h
	$(RM) $(INCLUDEDIR)/cups/help-index.h
	-$(RMDIR) $(INCLUDEDIR)/cups


#
# Automatic API help files...
#

apihelp:
	mxmldoc --section "Programming" \
		--title "CGI API" \
		--css ../doc/cups-printable.css \
		--header api-cgi.header --intro api-cgi.shtml \
		cgi.h help-index.h $(LIBOBJS:.o=.c) >../doc/help/api-cgi.html
	mxmldoc --tokens help/api-cgi.html api-cgi.xml >../doc/help/api-cgi.tokens
	$(RM) api-cgi.xml

framedhelp:
	mxmldoc --framed api-cgi \
		--section "Programming" \
		--title "CGI API" \
		--css ../doc/cups-printable.css \
		--header api-cgi.header --intro api-cgi.shtml \
		cgi.h help-index.h $(LIBOBJS:.o=.c)


#
# libcupscgi.so.1, libcupscgi.sl.1
#

libcupscgi.so.1 libcupscgi.sl.1:	$(LIBOBJS)
	echo Linking $@...
	$(DSO) $(ARCHFLAGS) $(DSOFLAGS) -o $@ $(LIBOBJS) $(LIBS)
	$(RM) `basename $@ .1`
	$(LN) $@ `basename $@ .1`


#
# libcupscgi.1.dylib
#

libcupscgi.1.dylib:	$(LIBOBJS) libcupscgi.exp
	echo Linking $@...
	$(DSO) $(ARCHFLAGS) $(DSOFLAGS) -o $@ \
		-install_name $(libdir)/$@ \
		-current_version 1.0.0 \
		-compatibility_version 1.0.0 \
		-exported_symbols_list libcupscgi.exp \
		$(LIBOBJS) $(LIBS)
	$(RM) libcupscgi.dylib
	$(LN) $@ libcupscgi.dylib


#
# libcupscgi_s.a
#

libcupscgi_s.a:	$(LIBOBJS)
	echo Creating $@...
	$(DSO) $(DSOFLAGS) -o libcupscgi_s.o $(LIBOBJS) $(LIBS)
	$(RM) $@
	$(AR) $(ARFLAGS) $@ libcupscgi_s.o


#
# libcupscgi.la
#

libcupscgi.la:    $(LIBOBJS)
	echo Linking $@...
	$(CC) $(ARCHFLAGS) $(DSOFLAGS) -o $@ $(LIBOBJS:.o=.lo) -rpath $(LIBDIR) \
		-version-info 1:0 $(LIBS)


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

admin.cgi:	admin.o ../Makedefs ../cups/$(LIBCUPS) $(LIBCUPSCGI)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o $@ admin.o -lcupscgi $(LIBS)


#
# classes.cgi
#

classes.cgi:	classes.o ../Makedefs ../cups/$(LIBCUPS) $(LIBCUPSCGI)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o $@ classes.o -lcupscgi $(LIBS)


#
# help.cgi
#

help.cgi:	help.o ../Makedefs ../cups/$(LIBCUPS) $(LIBCUPSCGI)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o $@ help.o -lcupscgi $(LIBS)


#
# jobs.cgi
#

jobs.cgi:	jobs.o ../Makedefs ../cups/$(LIBCUPS) $(LIBCUPSCGI)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o $@ jobs.o -lcupscgi $(LIBS)


#
# makedocset
#

makedocset:	makedocset.o ../Makedefs libcupscgi.a ../cups/$(LIBCUPSSTATIC)
	echo Linking $@...
	$(CC) $(ARCHFLAGS) $(LDFLAGS) -o $@ makedocset.o libcupscgi.a \
		../cups/$(LIBCUPSSTATIC) $(COMMONLIBS) $(SSLLIBS) $(DNSSDLIBS) \
		$(LIBZ) $(LIBGSSAPI)


#
# printers.cgi
#

printers.cgi:	printers.o ../Makedefs ../cups/$(LIBCUPS) $(LIBCUPSCGI)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o $@ printers.o -L. -lcupscgi $(LIBS)


#
# testcgi
#

testcgi:	testcgi.o ../Makedefs libcupscgi.a ../cups/$(LIBCUPSSTATIC)
	echo Linking $@...
	$(CC) $(ARCHFLAGS) $(LDFLAGS) -o $@ testcgi.o libcupscgi.a \
		../cups/$(LIBCUPSSTATIC) $(COMMONLIBS) $(SSLLIBS) $(DNSSDLIBS) \
		$(LIBZ) $(LIBGSSAPI)
	echo Testing CGI API...
	./testcgi


#
# testhi
#

testhi:	testhi.o ../Makedefs libcupscgi.a ../cups/$(LIBCUPSSTATIC)
	echo Linking $@...
	$(CC) $(ARCHFLAGS) $(LDFLAGS) -o $@ testhi.o libcupscgi.a \
		../cups/$(LIBCUPSSTATIC) $(COMMONLIBS) $(SSLLIBS) $(DNSSDLIBS) \
		$(LIBZ) $(LIBGSSAPI)
	echo Testing help index API...
	./testhi


#
# testtemplate
#

testtemplate:	testtemplate.o ../Makedefs libcupscgi.a ../cups/$(LIBCUPSSTATIC)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o $@ testtemplate.o libcupscgi.a ../cups/$(LIBCUPSSTATIC) \
		$(COMMONLIBS) $(SSLLIBS) $(DNSSDLIBS) $(LIBZ) $(LIBGSSAPI)


#
# websearch
#

websearch:	websearch.o ../Makedefs libcupscgi.a ../cups/$(LIBCUPSSTATIC)
	echo Linking $@...
	$(CC) $(ARCHFLAGS) $(LDFLAGS) -o $@ websearch.o libcupscgi.a \
		../cups/$(LIBCUPSSTATIC) $(COMMONLIBS) $(SSLLIBS) $(DNSSDLIBS) \
		$(LIBZ) $(LIBGSSAPI)


#
# Dependencies...
#

include Dependencies


#
# End of "$Id: Makefile 7871 2008-08-27 21:12:43Z mike $".
#
