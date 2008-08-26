#
# "$Id$"
#
#   Top-level Makefile for the Common UNIX Printing System (CUPS).
#
#   Copyright 2007-2008 by Apple Inc.
#   Copyright 1997-2007 by Easy Software Products, all rights reserved.
#
#   These coded instructions, statements, and computer programs are the
#   property of Apple Inc. and are protected by Federal copyright
#   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
#   which should have been included with this file.  If this file is
#   file is missing or damaged, see the license at "http://www.cups.org/".
#

include Makedefs

#
# Directories to make...
#

DIRS	=	cups filter backend berkeley cgi-bin driver locale man monitor \
		notifier ppdc scheduler systemv test \
		$(PHPDIR) \
		conf data doc $(FONTS) templates


#
# Make all targets...
#

all:
	chmod +x cups-config
	echo Using ARCHFLAGS="$(ARCHFLAGS)"
	echo Using ALL_CFLAGS="$(ALL_CFLAGS)"
	echo Using ALL_CXXFLAGS="$(ALL_CXXFLAGS)"
	echo Using DSOFLAGS="$(DSOFLAGS)"
	echo Using LDFLAGS="$(LDFLAGS)"
	echo Using LIBS="$(LIBS)"
	for dir in $(DIRS); do\
		echo Making all in $$dir... ;\
		(cd $$dir ; $(MAKE) $(MFLAGS) all) || exit 1;\
	done


#
# Make library targets...
#

libs:
	echo Using ARCHFLAGS="$(ARCHFLAGS)"
	echo Using ALL_CFLAGS="$(ALL_CFLAGS)"
	echo Using ALL_CXXFLAGS="$(ALL_CXXFLAGS)"
	echo Using DSOFLAGS="$(DSOFLAGS)"
	echo Using LDFLAGS="$(LDFLAGS)"
	echo Using LIBS="$(LIBS)"
	for dir in $(DIRS); do\
		echo Making libraries in $$dir... ;\
		(cd $$dir ; $(MAKE) $(MFLAGS) libs) || exit 1;\
	done


#
# Remove object and target files...
#

clean:
	for dir in $(DIRS); do\
		echo Cleaning in $$dir... ;\
		(cd $$dir; $(MAKE) $(MFLAGS) clean) || exit 1;\
	done


#
# Remove all non-distribution files...
#

distclean:	clean
	$(RM) Makedefs config.h config.log config.status
	$(RM) cups-config
	$(RM) conf/cupsd.conf conf/mime.convs conf/pam.std conf/snmp.conf
	$(RM) doc/help/ref-cupsd-conf.html doc/help/standard.html doc/index.html
	$(RM) init/cups.sh init/cups-lpd init/org.cups.cups-lpd.plist
	$(RM) man/client.conf.man
	$(RM) man/cups-deviced.man man/cups-driverd.man
	$(RM) man/cups-lpd.man man/cupsaddsmb.man man/cupsd.man
	$(RM) man/cupsd.conf.man man/lpoptions.man
	$(RM) packaging/cups.list
	$(RM) templates/header.tmpl
	-$(RM) doc/*/index.html
	-$(RM) templates/*/header.tmpl
	-$(RM) -r autom4te*.cache


#
# Make dependencies
#

depend:
	for dir in $(DIRS); do\
		echo Making dependencies in $$dir... ;\
		(cd $$dir; $(MAKE) $(MFLAGS) depend) || exit 1;\
	done


#
# Run the clang.llvm.org static code analysis tool on the C sources.
#

.PHONY: clang
clang:
	if test ! -d clang; then \
		mkdir clang; \
	else \
		rm -rf clang/*; \
	fi
	$(MAKE) $(MFLAGS) CC="scan-build -o ../clang $(CC)" \
		CXX="scan-build -o ../clang $(CXX)" clean all
	test `ls -1 clang | wc -l` = 0


#
# Generate a ctags file...
#

ctags:
	ctags -R .


#
# Install everything...
#

install:	install-data install-headers install-libs install-exec


#
# Install data files...
#

install-data:
	for dir in $(DIRS); do\
		echo Installing data files in $$dir... ;\
		(cd $$dir; $(MAKE) $(MFLAGS) install-data) || exit 1;\
	done
	echo Installing cups-config script...
	$(INSTALL_DIR) -m 755 $(BINDIR)
	$(INSTALL_SCRIPT) cups-config $(BINDIR)/cups-config
	if test "x$(INITDIR)" != x; then \
		echo Installing init scripts...; \
		$(INSTALL_DIR) -m 755 $(BUILDROOT)$(INITDIR)/init.d; \
		$(INSTALL_SCRIPT) init/cups.sh $(BUILDROOT)$(INITDIR)/init.d/cups; \
		for level in $(RCLEVELS); do \
			$(INSTALL_DIR) -m 755 $(BUILDROOT)$(INITDIR)/rc$${level}.d; \
			$(LN) ../init.d/cups $(BUILDROOT)$(INITDIR)/rc$${level}.d/S$(RCSTART)cups; \
			if test `uname` = HP-UX; then \
				level=`expr $$level - 1`; \
				$(INSTALL_DIR) -m 755 $(BUILDROOT)$(INITDIR)/rc$${level}.d; \
			fi; \
			$(LN) ../init.d/cups $(BUILDROOT)$(INITDIR)/rc$${level}.d/K$(RCSTOP)cups; \
		done; \
		$(INSTALL_DIR) -m 755 $(BUILDROOT)$(INITDIR)/rc0.d; \
		$(LN) ../init.d/cups $(BUILDROOT)$(INITDIR)/rc0.d/K$(RCSTOP)cups; \
	fi
	if test "x$(INITDIR)" = x -a "x$(INITDDIR)" != x; then \
		$(INSTALL_DIR) $(BUILDROOT)$(INITDDIR); \
		if test "$(INITDDIR)" = "/System/Library/LaunchDaemons"; then \
			echo Installing LaunchDaemons configuration files...; \
			$(INSTALL_DATA) init/org.cups.cupsd.plist $(BUILDROOT)$(DEFAULT_LAUNCHD_CONF); \
			$(INSTALL_DATA) init/org.cups.cups-lpd.plist $(BUILDROOT)/System/Library/LaunchDaemons; \
			case `uname -r` in \
				8.*) \
				$(INSTALL_DIR) $(BUILDROOT)/System/Library/StartupItems/PrintingServices; \
				$(INSTALL_SCRIPT) init/PrintingServices.launchd $(BUILDROOT)/System/Library/StartupItems/PrintingServices/PrintingServices; \
				$(INSTALL_DATA) init/StartupParameters.plist $(BUILDROOT)/System/Library/StartupItems/PrintingServices/StartupParameters.plist; \
				$(INSTALL_DIR) $(BUILDROOT)/System/Library/StartupItems/PrintingServices/Resources/English.lproj; \
				$(INSTALL_DATA) init/Localizable.strings $(BUILDROOT)/System/Library/StartupItems/PrintingServices/Resources/English.lproj/Localizable.strings; \
				;; \
			esac \
		else \
			echo Installing RC script...; \
			$(INSTALL_SCRIPT) init/cups.sh $(BUILDROOT)$(INITDDIR)/cups; \
		fi \
	fi
	if test "x$(SMFMANIFESTDIR)" != x; then \
		echo Installing SMF manifest in $(SMFMANIFESTDIR)...;\
		$(INSTALL_DIR) $(BUILDROOT)/$(SMFMANIFESTDIR); \
		$(INSTALL_SCRIPT) init/cups.xml $(BUILDROOT)$(SMFMANIFESTDIR)/cups.xml; \
	fi
	if test "x$(DBUSDIR)" != x; then \
		echo Installing cups.conf in $(DBUSDIR)...;\
		$(INSTALL_DIR) -m 755 $(BUILDROOT)$(DBUSDIR)/system.d; \
		$(INSTALL_DATA) packaging/cups-dbus.conf $(BUILDROOT)$(DBUSDIR)/system.d/cups.conf; \
	fi
	if test "x$(XINETD)" != x; then \
		echo Installing xinetd configuration file for cups-lpd...; \
		$(INSTALL_DIR) -m 755 $(BUILDROOT)$(XINETD); \
		$(INSTALL_DATA) init/cups-lpd $(BUILDROOT)$(XINETD)/cups-lpd; \
	fi
	if test "x$(MENUDIR)" != x; then \
		echo Installing desktop menu...; \
		$(INSTALL_DIR) -m 755 $(BUILDROOT)$(MENUDIR); \
		$(INSTALL_DATA) desktop/cups.desktop $(BUILDROOT)$(MENUDIR); \
	fi
	if test "x$(ICONDIR)" != x; then \
		echo Installing desktop icons...; \
		$(INSTALL_DIR) -m 755 $(BUILDROOT)$(ICONDIR)/hicolor/16x16/apps; \
		$(INSTALL_DATA) desktop/cups-16.png $(BUILDROOT)$(ICONDIR)/hicolor/16x16/apps/cups.png; \
		$(INSTALL_DIR) -m 755 $(BUILDROOT)$(ICONDIR)/hicolor/32x32/apps; \
		$(INSTALL_DATA) desktop/cups-32.png $(BUILDROOT)$(ICONDIR)/hicolor/32x32/apps/cups.png; \
		$(INSTALL_DIR) -m 755 $(BUILDROOT)$(ICONDIR)/hicolor/64x64/apps; \
		$(INSTALL_DATA) desktop/cups-64.png $(BUILDROOT)$(ICONDIR)/hicolor/64x64/apps/cups.png; \
		$(INSTALL_DIR) -m 755 $(BUILDROOT)$(ICONDIR)/hicolor/128x128/apps; \
		$(INSTALL_DATA) desktop/cups-128.png $(BUILDROOT)$(ICONDIR)/hicolor/128x128/apps/cups.png; \
	fi

#
# Install header files...
#

install-headers:
	for dir in $(DIRS); do\
		echo Installing header files in $$dir... ;\
		(cd $$dir; $(MAKE) $(MFLAGS) install-headers) || exit 1;\
	done


#
# Install programs...
#

install-exec:	all
	for dir in $(DIRS); do\
		echo Installing programs in $$dir... ;\
		(cd $$dir; $(MAKE) $(MFLAGS) install-exec) || exit 1;\
	done


#
# Install libraries...
#

install-libs:	libs
	for dir in $(DIRS); do\
		echo Installing libraries in $$dir... ;\
		(cd $$dir; $(MAKE) $(MFLAGS) install-libs) || exit 1;\
	done


#
# Uninstall object and target files...
#

uninstall:
	for dir in $(DIRS); do\
		echo Uninstalling in $$dir... ;\
		(cd $$dir; $(MAKE) $(MFLAGS) uninstall) || exit 1;\
	done
	echo Uninstalling cups-config script...
	$(RM) $(BINDIR)/cups-config
	-$(RMDIR) $(BINDIR)
	echo Uninstalling startup script...
	if test "x$(INITDIR)" != x; then \
		$(RM) $(BUILDROOT)$(INITDIR)/init.d/cups; \
		$(RMDIR) $(BUILDROOT)$(INITDIR)/init.d; \
		$(RM)  $(BUILDROOT)$(INITDIR)/rc0.d/K00cups; \
		$(RMDIR) $(BUILDROOT)$(INITDIR)/rc0.d; \
		$(RM) $(BUILDROOT)$(INITDIR)/rc2.d/S99cups; \
		$(RMDIR) $(BUILDROOT)$(INITDIR)/rc2.d; \
		$(RM) $(BUILDROOT)$(INITDIR)/rc3.d/S99cups; \
		$(RMDIR) $(BUILDROOT)$(INITDIR)/rc3.d; \
		$(RM) $(BUILDROOT)$(INITDIR)/rc5.d/S99cups; \
		$(RMDIR) $(BUILDROOT)$(INITDIR)/rc5.d; \
	fi
	if test "x$(INITDIR)" = x -a "x$(INITDDIR)" != x; then \
		if test "$(INITDDIR)" = "/System/Library/StartupItems/PrintingServices"; then \
			$(RM) $(BUILDROOT)$(INITDDIR)/PrintingServices; \
			$(RM) $(BUILDROOT)$(INITDDIR)/StartupParameters.plist; \
			$(RM) $(BUILDROOT)$(INITDDIR)/Resources/English.lproj/Localizable.strings; \
			$(RMDIR) $(BUILDROOT)$(INITDDIR)/Resources/English.lproj; \
		elif test "$(INITDDIR)" = "/System/Library/LaunchDaemons"; then \
			$(RM) $(BUILDROOT)$(INITDDIR)/org.cups.cupsd.plist; \
			$(RM) $(BUILDROOT)$(INITDDIR)/org.cups.cups-lpd.plist; \
			$(RMDIR) $(BUILDROOT)/System/Library/StartupItems/PrintingServices; \
		else \
			$(INSTALL_SCRIPT) init/cups.sh $(BUILDROOT)$(INITDDIR)/cups; \
		fi \
		$(RMDIR) $(BUILDROOT)$(INITDDIR); \
	fi
	if test "x$(DBUSDIR)" != x; then \
		echo Uninstalling cups.conf in $(DBUSDIR)...;\
		$(RM) $(BUILDROOT)$(DBUSDIR)/cups.conf; \
		$(RMDIR) $(BUILDROOT)$(DBUSDIR); \
	fi
	$(RM) $(BUILDROOT)/etc/xinetd.d/cups-lpd
	$(RM) $(BUILDROOT)/usr/share/applications/cups.desktop
	$(RM) $(BUILDROOT)/usr/share/icons/hicolor/16x16/apps/cups.png
	$(RM) $(BUILDROOT)/usr/share/icons/hicolor/32x32/apps/cups.png
	$(RM) $(BUILDROOT)/usr/share/icons/hicolor/64x64/apps/cups.png
	$(RM) $(BUILDROOT)/usr/share/icons/hicolor/128x128/apps/cups.png


#
# Run the test suite...
#

test:	all
	echo Running CUPS test suite...
	cd test; ./run-stp-tests.sh


check:	all
	echo Running CUPS test suite with defaults...
	cd test; ./run-stp-tests.sh 1 0 n


#
# Make software distributions using EPM (http://www.easysw.com/epm/)...
#

EPMFLAGS	=	-v --output-dir dist $(EPMARCH)

aix bsd deb depot inst pkg setld slackware swinstall tardist:
	epm $(EPMFLAGS) -f $@ cups packaging/cups.list

epm:
	epm $(EPMFLAGS) -s packaging/installer.gif cups packaging/cups.list

osx:
	epm $(EPMFLAGS) -f osx -s packaging/installer.tif cups packaging/cups.list

rpm:
	epm $(EPMFLAGS) -f rpm -s packaging/installer.gif cups packaging/cups.list

.PHONEY:	dist
dist:	all
	$(RM) -r dist
	$(MAKE) $(MFLAGS) epm
	case `uname` in \
		*BSD*) $(MAKE) $(MFLAGS) bsd;; \
		Darwin*) $(MAKE) $(MFLAGS) osx;; \
		IRIX*) $(MAKE) $(MFLAGS) tardist;; \
		Linux*) $(MAKE) $(MFLAGS) rpm;; \
		SunOS*) $(MAKE) $(MFLAGS) pkg;; \
	esac


#
# End of "$Id$".
#
