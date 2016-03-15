#
# "$Id: build.mak 10105 2011-11-04 16:32:00Z mike $"
#
#   Makefile for CUPS build and test repository.
#
#   This makefile MUST be used from the build.sh script, otherwise the
#   environment will not be configured properly...
#
#   Copyright 2007-2014 by Apple Inc.
#   Copyright 1997-2007 by Easy Software Products, all rights reserved.
#
#   These coded instructions, statements, and computer programs are the
#   property of Apple Inc. and are protected by Federal copyright
#   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
#   which should have been included with this file.  If this file is
#   file is missing or damaged, see the license at "http://www.cups.org/".

# Subdirectories to be built...
DIRS	=	\
		stable/.all \
		trunk/.all \


# Make everything...
all:	tools/.all $(DIRS)


# Clean everything...
clean:	tools/.clean $(DIRS:.all=.clean)
	@echo Removing \"temp\" directory...
	rm -rf temp
	@echo Removing old failed test reports...
	find . -name 'cups-str*.html' -mtime +7 -print -exec rm -f '{}' \;
	find . -name 'error_log-*' -mtime +7 -print -exec rm -f '{}' \;


# Make EPM
epm/.all:	epm/Makefile
	@echo Making all in EPM...
	cd epm && $(MAKE) test && $(MAKE) install

epm/.clean:
	@cd epm; if test -f Makefile; then \
		echo Cleaning EPM...; \
		$(MAKE) distclean || exit 1; \
	fi

epm/Makefile:	epm/Makefile.in epm/configure
	@cd epm; if test -f Makefile; then \
		echo Cleaning EPM...; \
		$(MAKE) distclean || exit 1; \
	fi
	@echo Configuring EPM...
	cd epm && ./configure $(BUILDOPTIONS) --prefix=$(BASEDIR)/temp


# Make CUPS stable
stable/.all:	stable/Makedefs
	@echo Making all in CUPS stable...
	cd stable && $(MAKE) all check
#	@if test -x /usr/bin/rpm; then \
#		cd stable && tools/testrpm; \
#	fi

stable/.clean:
	@cd stable; if test -f Makedefs; then \
		echo Cleaning CUPS stable...; \
		$(MAKE) distclean || exit 1; \
	fi

stable/Makedefs:	stable/Makedefs.in stable/configure \
			stable/packaging/cups.list.in \
			stable/config.h.in
	@cd stable; if test -f Makedefs; then \
		echo Cleaning CUPS stable...; \
		$(MAKE) distclean || exit 1; \
	fi
	@echo Configuring CUPS stable...
	cd stable && ./configure $(BUILDOPTIONS) \
		--enable-static \
		--enable-unit-tests --enable-debug-printfs

stable/configure: stable/configure.ac \
	stable/config-scripts/cups-common.m4 \
	stable/config-scripts/cups-compiler.m4 \
	stable/config-scripts/cups-defaults.m4 \
	stable/config-scripts/cups-directories.m4 \
	stable/config-scripts/cups-dnssd.m4 \
	stable/config-scripts/cups-gssapi.m4 \
	stable/config-scripts/cups-largefile.m4 \
	stable/config-scripts/cups-libtool.m4 \
	stable/config-scripts/cups-manpages.m4 \
	stable/config-scripts/cups-network.m4 \
	stable/config-scripts/cups-opsys.m4 \
	stable/config-scripts/cups-pam.m4 \
	stable/config-scripts/cups-poll.m4 \
	stable/config-scripts/cups-scripting.m4 \
	stable/config-scripts/cups-sharedlibs.m4 \
	stable/config-scripts/cups-ssl.m4 \
	stable/config-scripts/cups-startup.m4 \
	stable/config-scripts/cups-threads.m4
	@echo Updating CUPS stable configure script...
	cd stable; rm -rf autom4te.cache configure; autoconf


# Make CUPS trunk
trunk/.all:	trunk/Makedefs
	@echo Making all in CUPS trunk...
	cd trunk && $(MAKE) all check
#	@if test -x /usr/bin/rpm; then \
#		cd trunk && tools/testrpm; \
#	fi

trunk/.clean:
	@cd trunk; if test -f Makedefs; then \
		echo Cleaning CUPS trunk...; \
		$(MAKE) distclean || exit 1; \
	fi

trunk/Makedefs:	trunk/Makedefs.in trunk/configure \
			trunk/packaging/cups.list.in \
			trunk/config.h.in
	@cd trunk; if test -f Makedefs; then \
		echo Cleaning CUPS trunk...; \
		$(MAKE) distclean || exit 1; \
	fi
	@echo Configuring CUPS trunk...
	cd trunk && ./configure $(BUILDOPTIONS) \
		--enable-static \
		--enable-unit-tests --enable-debug-printfs

trunk/configure: trunk/configure.ac \
	trunk/config-scripts/cups-common.m4 \
	trunk/config-scripts/cups-compiler.m4 \
	trunk/config-scripts/cups-defaults.m4 \
	trunk/config-scripts/cups-directories.m4 \
	trunk/config-scripts/cups-dnssd.m4 \
	trunk/config-scripts/cups-gssapi.m4 \
	trunk/config-scripts/cups-largefile.m4 \
	trunk/config-scripts/cups-libtool.m4 \
	trunk/config-scripts/cups-manpages.m4 \
	trunk/config-scripts/cups-network.m4 \
	trunk/config-scripts/cups-opsys.m4 \
	trunk/config-scripts/cups-pam.m4 \
	trunk/config-scripts/cups-poll.m4 \
	trunk/config-scripts/cups-scripting.m4 \
	trunk/config-scripts/cups-sharedlibs.m4 \
	trunk/config-scripts/cups-ssl.m4 \
	trunk/config-scripts/cups-startup.m4 \
	trunk/config-scripts/cups-threads.m4
	@echo Updating CUPS trunk configure script...
	cd trunk; rm -rf autom4te.cache configure; autoconf

# Make the build tools
tools/.all:	tools/Makefile
	@echo Making all in tools...
	cd tools && $(MAKE) install

tools/.clean:
	@cd tools; if test -f Makefile; then \
		echo Cleaning tools...; \
		$(MAKE) distclean || exit 1; \
	fi

tools/Makefile:	tools/Makefile.in tools/configure
	@cd tools; if test -f Makefile; then \
		echo Cleaning tools...; \
		$(MAKE) distclean || exit 1; \
	fi
	@echo Configuring Tools...
	cd tools && ./configure $(BUILDOPTIONS) --prefix=$(BASEDIR)/temp


#
# End of "$Id: build.mak 10105 2011-11-04 16:32:00Z mike $".
#
