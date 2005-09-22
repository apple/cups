#
# "$Id$"
#
#   API library Makefile for the Common UNIX Printing System (CUPS).
#
#   Copyright 1997-2005 by Easy Software Products, all rights reserved.
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
#   This file is subject to the Apple OS-Developed Software exception.
#

include ../Makedefs

#
# Object files...
#

LIBOBJS	=	\
		array.o \
		attr.o \
		auth.o \
		backchannel.o \
		dest.o \
		dir.o \
		emit.o \
		encode.o \
		file.o \
		getputfile.o \
		globals.o \
		http.o \
		http-addr.o \
		http-support.o \
		ipp.o \
		ipp-support.o \
		langprintf.o \
		language.o \
		mark.o \
		md5.o \
		md5passwd.o \
		normalize.o \
		options.o \
		page.o \
		ppd.o \
		snprintf.o \
		string.o \
		tempfile.o \
		transcode.o \
		usersys.o \
		util.o
OBJS	=	\
		$(LIBOBJS) \
		testhttp.o \
		testi18n.o \
		testipp.o \
		testlang.o \
		php_cups_wrap.o


#
# Header files to install...
#

HEADERS	=	\
		array.h \
		cups.h \
		dir.h \
		file.h \
		http.h \
		ipp.h \
		language.h \
		md5.h \
		normalize.h \
		ppd.h \
		transcode.h


#
# Targets in this directory...
#

TARGETS	=	\
		$(LIBCUPS) \
		libcups.a \
		testhttp \
		testi18n \
		testipp \
		testlang


#
# Make all targets...
#

all:	$(TARGETS)


#
# Remove object and target files...
#

clean:
	$(RM) $(OBJS) $(TARGETS) `basename $(LIBCUPS) .2` libcups.dylib


#
# Update dependencies (without system header dependencies...)
#

depend:
	makedepend -Y -I.. -fDependencies $(OBJS:.o=.c) >/dev/null 2>&1


#
# Install object and target files...
#

install:	all	installhdrs
	$(INSTALL_DIR) $(LIBDIR)
	$(INSTALL_LIB) $(LIBCUPS) $(LIBDIR)
	if test $(LIBCUPS) = "libcups.so.2" -o $(LIBCUPS) = "libcups.sl.2"; then \
		$(RM) $(LIBDIR)/`basename $(LIBCUPS) .2`; \
		$(LN) $(LIBCUPS) $(LIBDIR)/`basename $(LIBCUPS) .2`; \
	fi
	if test $(LIBCUPS) = "libcups.2.dylib"; then \
		$(STRIP) -x $(LIBDIR)/$(LIBCUPS); \
		$(RM) $(LIBDIR)/libcups.dylib; \
		$(LN) $(LIBCUPS) $(LIBDIR)/libcups.dylib; \
	fi
	if test $(LIBCUPS) != "libcups.a"; then \
		$(INSTALL_LIB) libcups.a $(LIBDIR); \
		$(RANLIB) $(LIBDIR)/libcups.a; \
	fi

installhdrs:
	$(INSTALL_DIR) $(INCLUDEDIR)/cups
	for file in $(HEADERS); do \
		$(INSTALL_DATA) $$file $(INCLUDEDIR)/cups; \
	done


#
# libcups.so.2, libcups.sl.2
#

libcups.so.2 libcups.sl.2:	$(LIBOBJS)
	echo Linking $@...
	$(DSO) $(DSOFLAGS) -o $@ $(LIBOBJS) $(SSLLIBS) $(COMMONLIBS) $(LIBZ)
	$(RM) `basename $@ .2`
	$(LN) $@ `basename $@ .2`


#
# libcups.2.dylib
#

libcups.2.dylib:	$(LIBOBJS)
	echo Linking $@...
	$(DSO) $(DSOFLAGS) -o $@ \
		-install_name $(libdir)/libcups.dylib \
		-current_version 2.7.0 \
		-compatibility_version 2.0.0 \
		$(LIBOBJS) $(SSLLIBS) $(COMMONLIBS) $(LIBZ)
	$(RM) libcups.dylib
	$(LN) $@ libcups.dylib


#
# libcups_s.a
#

libcups_s.a:	$(LIBOBJS)
	echo Creating $@...
	$(RM) libcups_s.exp
	(echo _ipp_add_attr; echo _ipp_free_attr; \
	 echo _cups_hstrerror; echo _cups_md5_init; \
	 echo _cups_md5_append; echo _cups_md5_finish; \
	 echo _cups_strlcat; echo _cups_strlcpy) >libcups_s.exp
	$(DSO) $(DSOFLAGS) -Wl,-bexport:libcups_s.exp -o libcups_s.o $(LIBOBJS) $(SSLLIBS) $(COMMONLIBS) $(LIBZ) -lm
	$(RM) $@
	$(AR) $(ARFLAGS) $@ libcups_s.o


#
# libcups.la
#

libcups.la:    $(LIBOBJS)
	echo Linking $@...
	$(CC) $(DSOFLAGS) -o $@ $(LIBOBJS:.o=.lo) -rpath $(LIBDIR) \
		-version-info 2:7 $(SSLLIBS) $(COMMONLIBS) $(LIBZ)


#
# libcups.a
#

libcups.a:	$(LIBOBJS)
	echo Archiving $@...
	$(RM) $@
	$(AR) $(ARFLAGS) $@ $(LIBOBJS)
	$(RANLIB) $@


#
# CUPS language bindings for various scripting languages...
#

phpcups.so:	$(LIBCUPS) php_cups_wrap.o
	echo Linking $@...
	if test `uname` = Darwin; then \
		DSOFLAGS="-bundle -flat_namespace -undefined suppress"; \
	else \
		DSOFLAGS="$(DSOFLAGS)"; \
	fi; \
	$(DSO) $$DSOFLAGS -o $@ php_cups_wrap.o $(LIBS) `php-config --ldflags --libs`

php_cups_wrap.o:	php_cups_wrap.c
	echo Compiling $<...
	$(CC) $(CFLAGS) `php-config --includes` -c $<
php_cups_wrap.c:	cups.h
	echo Creating $< using SWIG...
	swig -php -o $@ -module cups cups.h


#
# cups_C.h - the default POSIX locale that is compiled in.
#

cups_C.h:	../locale/C/cups_C
	echo Generating $@...
	$(RM) cups_C.h
	$(AWK) '{print "\"" $$0 "\","}' < ../locale/C/cups_C > cups_C.h


#
# testhttp (dependency on static CUPS library is intentional)
#

testhttp:	testhttp.o libcups.a
	echo Linking $@...
	$(CC) $(LDFLAGS) -o $@ testhttp.o libcups.a $(NETLIBS) $(SSLLIBS) $(COMMONLIBS)


#
# testipp (dependency on static CUPS library is intentional)
#

testipp:	testipp.o libcups.a
	echo Linking $@...
	$(CC) $(LDFLAGS) -o $@ testipp.o libcups.a $(NETLIBS) $(SSLLIBS) $(COMMONLIBS)


#
# testi18n (dependency on static CUPS library is intentional)
#

testi18n:	testi18n.o libcups.a
	echo Linking $@...
	$(CC) $(LDFLAGS) -o $@ testi18n.o libcups.a $(NETLIBS) $(SSLLIBS) $(COMMONLIBS)


#
# testlang (dependency on static CUPS library is intentional)
#

testlang:	testlang.o libcups.a
	echo Linking $@...
	$(CC) $(LDFLAGS) -o $@ testlang.o libcups.a $(NETLIBS) $(SSLLIBS) $(COMMONLIBS)


#
# Automatic API help files...
#

apihelp:
	echo Generating CUPS API help files...
	mxmldoc --title "Array API" array.h array.c >../doc/help/api-array.html
	mxmldoc --title "File and Directory APIs" file.h file.c dir.h dir.c >../doc/help/api-filedir.html
	mxmldoc --title "PPD API" ppd.h attr.c emit.c mark.c page.c ppd.c >../doc/help/api-ppd.html
	mxmldoc --title "HTTP and IPP APIs" cups.h http.h ipp.h \
		auth.c dest.c encode.c getputfile.c http.c http-addr.c \
		http-support.c ipp.c ipp-support.c md5passwd.c options.c \
		usersys.c util.c >../doc/help/api-httpipp.html
	mxmldoc --title "Filter and Backend APIs" backchannel.c >../doc/help/api-filter.html


#
# Dependencies...
#

include Dependencies


#
# End of "$Id$".
#
