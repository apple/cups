#
# Filter makefile for CUPS.
#
# Copyright © 2007-2018 by Apple Inc.
# Copyright © 1997-2006 by Easy Software Products.
#
# Licensed under Apache License v2.0.  See the file "LICENSE" for more
# information.
#

include ../Makedefs


TARGETS	=	\
		commandtops \
		gziptoany \
		pstops \
		rastertoepson \
		rastertohp \
		rastertolabel \
		rastertopwg

OBJS	=	commandtops.o gziptoany.o common.o pstops.o \
		rastertoepson.o rastertohp.o rastertolabel.o \
		rastertopwg.o


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

unittests:


#
# Clean all object files...
#

clean:
	$(RM) $(OBJS) $(TARGETS)


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
	$(INSTALL_DIR) -m 755 $(SERVERBIN)/filter
	for file in $(TARGETS); do \
		$(INSTALL_BIN) $$file $(SERVERBIN)/filter; \
	done
	if test "x$(SYMROOT)" != "x"; then \
		$(INSTALL_DIR) $(SYMROOT); \
		for file in $(TARGETS); do \
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
	for file in $(TARGETS); do \
		$(RM) $(SERVERBIN)/filter/$$file; \
	done
	-$(RMDIR) $(SERVERBIN)/filter
	-$(RMDIR) $(SERVERBIN)


#
# Automatic API help files...
#

apihelp:
	echo Generating CUPS API help files...
	mxmldoc --section "Programming" \
		--title "Developing PostScript Printer Drivers" \
		--css ../doc/cups-printable.css \
		--header postscript-driver.header \
		--intro postscript-driver.shtml \
		>../doc/help/postscript-driver.html
	mxmldoc --section "Programming" \
		--title "Introduction to the PPD Compiler" \
		--css ../doc/cups-printable.css \
		--header ppd-compiler.header \
		--intro ppd-compiler.shtml \
		>../doc/help/ppd-compiler.html
	mxmldoc --section "Programming" \
		--title "Developing Raster Printer Drivers" \
		--css ../doc/cups-printable.css \
		--header raster-driver.header \
		--intro raster-driver.shtml \
		>../doc/help/raster-driver.html
	mxmldoc --section "Specifications" \
		--title "CUPS PPD Extensions" \
		--css ../doc/cups-printable.css \
		--header spec-ppd.header \
		--intro spec-ppd.shtml \
		>../doc/help/spec-ppd.html


#
# commandtops
#

commandtops:	commandtops.o ../cups/$(LIBCUPS)
	echo Linking $@...
	$(LD_CC) $(LDFLAGS) -o $@ commandtops.o $(LIBS)
	$(CODE_SIGN) -s "$(CODE_SIGN_IDENTITY)" $@


#
# gziptoany
#

gziptoany:	gziptoany.o ../Makedefs ../cups/$(LIBCUPS)
	echo Linking $@...
	$(LD_CC) $(LDFLAGS) -o $@ gziptoany.o $(LIBZ) $(LIBS)
	$(CODE_SIGN) -s "$(CODE_SIGN_IDENTITY)" $@


#
# pstops
#

pstops:	pstops.o common.o ../cups/$(LIBCUPS)
	echo Linking $@...
	$(LD_CC) $(LDFLAGS) -o $@ pstops.o common.o $(LIBS)
	$(CODE_SIGN) -s "$(CODE_SIGN_IDENTITY)" $@


#
# rastertoepson
#

rastertoepson:	rastertoepson.o ../cups/$(LIBCUPS) ../cups/$(LIBCUPSIMAGE)
	echo Linking $@...
	$(LD_CC) $(LDFLAGS) -o $@ rastertoepson.o $(LINKCUPSIMAGE) $(IMGLIBS) $(LIBS)
	$(CODE_SIGN) -s "$(CODE_SIGN_IDENTITY)" $@


#
# rastertohp
#

rastertohp:	rastertohp.o ../cups/$(LIBCUPS) ../cups/$(LIBCUPSIMAGE)
	echo Linking $@...
	$(LD_CC) $(LDFLAGS) -o $@ rastertohp.o $(LINKCUPSIMAGE) $(IMGLIBS) $(LIBS)
	$(CODE_SIGN) -s "$(CODE_SIGN_IDENTITY)" $@


#
# rastertolabel
#

rastertolabel:	rastertolabel.o ../cups/$(LIBCUPS) ../cups/$(LIBCUPSIMAGE)
	echo Linking $@...
	$(LD_CC) $(LDFLAGS) -o $@ rastertolabel.o $(LINKCUPSIMAGE) $(IMGLIBS) $(LIBS)
	$(CODE_SIGN) -s "$(CODE_SIGN_IDENTITY)" $@


#
# rastertopwg
#

rastertopwg:	rastertopwg.o ../cups/$(LIBCUPS) ../cups/$(LIBCUPSIMAGE)
	echo Linking $@...
	$(LD_CC) $(LDFLAGS) -o $@ rastertopwg.o $(LINKCUPSIMAGE) $(IMGLIBS) $(LIBS)
	$(CODE_SIGN) -s "$(CODE_SIGN_IDENTITY)" $@

rastertopwg-static:	rastertopwg.o ../cups/$(LIBCUPSSTATIC) ../cups/libcupsimage.a
	echo Linking $@...
	$(LD_CC) $(LDFLAGS) -o $@ rastertopwg.o ../cups/libcupsimage.a \
		../cups/$(LIBCUPSSTATIC) $(IMGLIBS) $(DSOLIBS) $(COMMONLIBS) \
		$(SSLLIBS) $(DNSSDLIBS) $(LIBGSSAPI)
	$(CODE_SIGN) -s "$(CODE_SIGN_IDENTITY)" $@


#
# Dependencies...
#

include Dependencies
