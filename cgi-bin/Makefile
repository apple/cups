#
# "$Id$"
#
#   CGI makefile for the Common UNIX Printing System (CUPS).
#
#   Copyright 2007-2008 by Apple Inc.
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
LIB32OBJS	= $(LIBOBJS:.o=.32.o)
LIB64OBJS	= $(LIBOBJS:.o=.64.o)
OBJS	=	\
		$(LIBOBJS) \
		$(LIB32OBJS) \
		$(LIB64OBJS) \
		admin.o \
		classes.o \
		help.o \
		jobs.o \
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
TARGETS	=	\
		libcupscgi.a \
		$(LIBCUPSCGI) \
		$(LIB32CUPSCGI) \
		$(LIB32CUPSCGI) \
		$(CGIS) \
		testcgi \
		testhi \
		testtemplate


#
# Make all targets...
#

all:	$(TARGETS)


#
# Clean all object files...
#

clean:
	$(RM) $(OBJS) $(TARGETS)
	$(RM) libcupscgi.so libcupscgi.sl libcupscgi.dylib
	$(RM) -r 32bit 64bit


#
# Update dependencies (without system header dependencies...)
#

depend:
	touch Dependencies.tmp
	makedepend -Y -I.. -fDependencies.tmp $(OBJS:.o=.c) >/dev/null 2>&1
	$(RM) Dependencies
	cp Dependencies.tmp Dependencies
	sed -E -e '1,$$s/^([^.]+)\.o:/\1\.32.o: \1\.c /' Dependencies.tmp >>Dependencies
	sed -E -e '1,$$s/^([^.]+)\.o:/\1\.64.o: \1\.c /' Dependencies.tmp >>Dependencies
	$(RM) Dependencies.tmp


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
		for file in $(TARGETS); do \
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

install-libs: $(INSTALLSTATIC) $(INSTALL32) $(INSTALL64)
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
	$(INSTALL_LIB) libcupscgi.a $(LIBDIR)
	$(RANLIB) $(LIBDIR)/libcupscgi.a

install32bit:
	echo Installing libraries in $(LIB32DIR)...
	$(INSTALL_DIR) -m 755 $(LIB32DIR)
	$(INSTALL_LIB) 32bit/libcupscgi.so.1 $(LIB32DIR)/libcupscgi.so.1
	$(LN) libcupscgi.so.1 $(LIB32DIR)/libcupscgi.so

install64bit:
	echo Installing libraries in $(LIB64DIR)...
	$(INSTALL_DIR) -m 755 $(LIB64DIR)
	$(INSTALL_LIB) 64bit/libcupscgi.so.1 $(LIB64DIR)/libcupscgi.so.1
	$(LN) libcupscgi.so.1 $(LIB64DIR)/libcupscgi.so


#
# Uninstall all targets...
#

uninstall: $(UNINSTALL32) $(UNINSTALL64)
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

uninstall32bit:
	$(RM) $(LIB32DIR)/libcupscgi.so
	$(RM) $(LIB32DIR)/libcupscgi.so.1
	-$(RMDIR) $(LIB32DIR)

uninstall64bit:
	$(RM) $(LIB64DIR)/libcupscgi.so
	$(RM) $(LIB64DIR)/libcupscgi.so.1
	-$(RMDIR) $(LIB64DIR)


#
# Automatic API help files...
#

apihelp:
	mxmldoc --section "Programming" \
		--title "CGI API" \
		--css ../doc/cups-printable.css \
		--header api-cgi.header --intro api-cgi.shtml \
		cgi.h help-index.h $(LIBOBJS:.o=.c) >../doc/help/api-cgi.html

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
# 32bit/libcupscgi.so.1
#

32bit/libcupscgi.so.1:	$(LIB32OBJS)
	echo Linking 32-bit $@...
	-mkdir 32bit
	$(DSO) $(ARCH32FLAGS) $(DSO32FLAGS) -o $@ $(LIB32OBJS) $(LIBS)
	$(RM) 32bit/libcupscgi.so
	$(LN) libcupscgi.so.1 32bit/libcupscgi.so


#
# 64bit/libcupscgi.so.1
#

64bit/libcupscgi.so.1:	$(LIB64OBJS)
	echo Linking 64-bit $@...
	-mkdir 64bit
	$(DSO) $(ARCH64FLAGS) $(DSO64FLAGS) -o $@ $(LIB64OBJS) $(LIBS)
	$(RM) 64bit/libcupscgi.so
	$(LN) libcupscgi.so.1 64bit/libcupscgi.so


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
	$(CC) $(LDFLAGS) -o $@ admin.o -L. -lcupscgi $(LIBS)


#
# classes.cgi
#

classes.cgi:	classes.o ../Makedefs ../cups/$(LIBCUPS) $(LIBCUPSCGI)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o $@ classes.o -L. -lcupscgi $(LIBS)


#
# help.cgi
#

help.cgi:	help.o ../Makedefs ../cups/$(LIBCUPS) $(LIBCUPSCGI)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o $@ help.o -L. -lcupscgi $(LIBS)


#
# jobs.cgi
#

jobs.cgi:	jobs.o ../Makedefs ../cups/$(LIBCUPS) $(LIBCUPSCGI)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o $@ jobs.o -L. -lcupscgi $(LIBS)


#
# printers.cgi
#

printers.cgi:	printers.o ../Makedefs ../cups/$(LIBCUPS) $(LIBCUPSCGI)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o $@ printers.o -L. -lcupscgi $(LIBS)


#
# testcgi
#

testcgi:	testcgi.o ../Makedefs libcupscgi.a ../cups/libcups.a
	echo Linking $@...
	$(CC) $(LDFLAGS) -o $@ testcgi.o libcupscgi.a ../cups/libcups.a \
		$(COMMONLIBS) $(SSLLIBS) $(LIBZ) $(LIBGSSAPI)


#
# testhi
#

testhi:	testhi.o ../Makedefs libcupscgi.a ../cups/libcups.a
	echo Linking $@...
	$(CC) $(LDFLAGS) -o $@ testhi.o libcupscgi.a ../cups/libcups.a \
		$(COMMONLIBS) $(SSLLIBS) $(LIBZ) $(LIBGSSAPI)


#
# testtemplate
#

testtemplate:	testtemplate.o ../Makedefs libcupscgi.a ../cups/libcups.a
	echo Linking $@...
	$(CC) $(LDFLAGS) -o $@ testtemplate.o libcupscgi.a ../cups/libcups.a \
		$(COMMONLIBS) $(SSLLIBS) $(LIBZ) $(LIBGSSAPI)


#
# Dependencies...
#

include Dependencies


#
# End of "$Id$".
#
