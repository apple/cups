#
# "$Id: Makefile 12414 2015-01-21 00:02:04Z msweet $"
#
# Top-level Makefile for CUPS.
#
# Copyright 2007-2014 by Apple Inc.
# Copyright 1997-2007 by Easy Software Products, all rights reserved.
#
# These coded instructions, statements, and computer programs are the
# property of Apple Inc. and are protected by Federal copyright
# law.  Distribution and use rights are outlined in the file "LICENSE.txt"
# which should have been included with this file.  If this file is
# file is missing or damaged, see the license at "http://www.cups.org/".
#

include Makedefs


#
# Directories to make...
#

DIRS	=	cups test $(BUILDDIRS)


#
# Make all targets...
#

all:
	chmod +x cups-config
	echo Using ARCHFLAGS="$(ARCHFLAGS)"
	echo Using ALL_CFLAGS="$(ALL_CFLAGS)"
	echo Using ALL_CXXFLAGS="$(ALL_CXXFLAGS)"
	echo Using CC="$(CC)"
	echo Using CXX="$(CC)"
	echo Using DSOFLAGS="$(DSOFLAGS)"
	echo Using LDFLAGS="$(LDFLAGS)"
	echo Using LIBS="$(LIBS)"
	for dir in $(DIRS); do\
		echo Making all in $$dir... ;\
		(cd $$dir ; $(MAKE) $(MFLAGS) all $(UNITTESTS)) || exit 1;\
	done


#
# Make library targets...
#

libs:
	echo Using ARCHFLAGS="$(ARCHFLAGS)"
	echo Using ALL_CFLAGS="$(ALL_CFLAGS)"
	echo Using ALL_CXXFLAGS="$(ALL_CXXFLAGS)"
	echo Using CC="$(CC)"
	echo Using CXX="$(CC)"
	echo Using DSOFLAGS="$(DSOFLAGS)"
	echo Using LDFLAGS="$(LDFLAGS)"
	echo Using LIBS="$(LIBS)"
	for dir in $(DIRS); do\
		echo Making libraries in $$dir... ;\
		(cd $$dir ; $(MAKE) $(MFLAGS) libs) || exit 1;\
	done


#
# Make unit test targets...
#

unittests:
	echo Using ARCHFLAGS="$(ARCHFLAGS)"
	echo Using ALL_CFLAGS="$(ALL_CFLAGS)"
	echo Using ALL_CXXFLAGS="$(ALL_CXXFLAGS)"
	echo Using CC="$(CC)"
	echo Using CXX="$(CC)"
	echo Using DSOFLAGS="$(DSOFLAGS)"
	echo Using LDFLAGS="$(LDFLAGS)"
	echo Using LIBS="$(LIBS)"
	for dir in $(DIRS); do\
		echo Making all in $$dir... ;\
		(cd $$dir ; $(MAKE) $(MFLAGS) unittests) || exit 1;\
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
	$(RM) conf/cups-files.conf conf/cupsd.conf conf/mime.convs conf/pam.std conf/snmp.conf
	$(RM) cups-config
	$(RM) data/testprint
	$(RM) desktop/cups.desktop
	$(RM) doc/index.html
	$(RM) man/client.conf.man man/cups-files.conf.man man/cups-lpd.man man/cups-snmp.man man/cupsaddsmb.man man/cupsd.conf.man man/cupsd.man man/lpoptions.man
	$(RM) packaging/cups.list
	$(RM) scheduler/cups-lpd.xinetd scheduler/cups.sh scheduler/cups.xml scheduler/org.cups.cups-lpd.plist scheduler/org.cups.cups-lpdAT.service scheduler/org.cups.cupsd.path scheduler/org.cups.cupsd.service scheduler/org.cups.cupsd.socket
	$(RM) templates/header.tmpl
	-$(RM) doc/*/index.html
	-$(RM) templates/*/header.tmpl
	-$(RM) -r autom4te*.cache clang cups/charmaps cups/locale


#
# Make dependencies
#

depend:
	for dir in $(DIRS); do\
		echo Making dependencies in $$dir... ;\
		(cd $$dir; $(MAKE) $(MFLAGS) depend) || exit 1;\
	done


#
# Run the Clang static code analysis tool on the sources, available here:
#
#    http://clang-analyzer.llvm.org
#
# At least checker-231 is required.
#

.PHONY: clang clang-changes
clang:
	$(RM) -r clang
	scan-build -V -k -o `pwd`/clang $(MAKE) $(MFLAGS) clean all
clang-changes:
	scan-build -V -k -o `pwd`/clang $(MAKE) $(MFLAGS) all


#
# Run the STACK tool on the sources, available here:
#
#    http://css.csail.mit.edu/stack/
#
# Do the following to pass options to configure:
#
#    make CONFIGFLAGS="--foo --bar" stack
#

.PHONY: stack
stack:
	stack-build ./configure $(CONFIGFLAGS)
	stack-build $(MAKE) $(MFLAGS) clean all
	poptck
	$(MAKE) $(MFLAGS) distclean
	$(RM) */*.ll
	$(RM) */*.ll.out


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
	echo Making all in cups...
	(cd cups; $(MAKE) $(MFLAGS) all)
	for dir in $(DIRS); do\
		echo Installing data files in $$dir... ;\
		(cd $$dir; $(MAKE) $(MFLAGS) install-data) || exit 1;\
	done
	echo Installing cups-config script...
	$(INSTALL_DIR) -m 755 $(BINDIR)
	$(INSTALL_SCRIPT) cups-config $(BINDIR)/cups-config


#
# Install header files...
#

install-headers:
	for dir in $(DIRS); do\
		echo Installing header files in $$dir... ;\
		(cd $$dir; $(MAKE) $(MFLAGS) install-headers) || exit 1;\
	done
	if test "x$(privateinclude)" != x; then \
		echo Installing config.h into $(PRIVATEINCLUDE)...; \
		$(INSTALL_DIR) -m 755 $(PRIVATEINCLUDE); \
		$(INSTALL_DATA) config.h $(PRIVATEINCLUDE)/config.h; \
	fi


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


#
# Run the test suite...
#

test:	all unittests
	echo Running CUPS test suite...
	cd test; ./run-stp-tests.sh


check:	all unittests
	echo Running CUPS test suite with defaults...
	cd test; ./run-stp-tests.sh 1 0 n n

debugcheck:	all unittests
	echo Running CUPS test suite with debug printfs...
	cd test; ./run-stp-tests.sh 1 0 n y


#
# Create HTML documentation using Mini-XML's mxmldoc (http://www.msweet.org/)...
#

apihelp:
	for dir in cgi-bin cups filter ppdc scheduler; do\
		echo Generating API help in $$dir... ;\
		(cd $$dir; $(MAKE) $(MFLAGS) apihelp) || exit 1;\
	done

framedhelp:
	for dir in cgi-bin cups filter ppdc scheduler; do\
		echo Generating framed API help in $$dir... ;\
		(cd $$dir; $(MAKE) $(MFLAGS) framedhelp) || exit 1;\
	done


#
# Create an Xcode docset using Mini-XML's mxmldoc (http://www.msweet.org/)...
#

docset:	apihelp
	echo Generating docset directory tree...
	$(RM) -r org.cups.docset
	mkdir -p org.cups.docset/Contents/Resources/Documentation/help
	mkdir -p org.cups.docset/Contents/Resources/Documentation/images
	cd man; $(MAKE) $(MFLAGS) html
	cd doc; $(MAKE) $(MFLAGS) docset
	cd cgi-bin; $(MAKE) $(MFLAGS) makedocset
	cgi-bin/makedocset org.cups.docset \
		`svnversion . | sed -e '1,$$s/[a-zA-Z]//g'` \
		doc/help/api-*.tokens
	$(RM) doc/help/api-*.tokens
	echo Indexing docset...
	/Applications/Xcode.app/Contents/Developer/usr/bin/docsetutil index org.cups.docset
	echo Generating docset archive and feed...
	$(RM) org.cups.docset.atom
	/Applications/Xcode.app/Contents/Developer/usr/bin/docsetutil package --output org.cups.docset.xar \
		--atom org.cups.docset.atom \
		--download-url http://www.cups.org/org.cups.docset.xar \
		org.cups.docset


#
# Lines of code computation...
#

sloc:
	for dir in cups scheduler; do \
		(cd $$dir; $(MAKE) $(MFLAGS) sloc) || exit 1;\
	done


#
# Make software distributions using EPM (http://www.msweet.org/)...
#

EPMFLAGS	=	-v --output-dir dist $(EPMARCH)

bsd deb pkg slackware:
	epm $(EPMFLAGS) -f $@ cups packaging/cups.list

epm:
	epm $(EPMFLAGS) -s packaging/installer.gif cups packaging/cups.list

rpm:
	epm $(EPMFLAGS) -f rpm -s packaging/installer.gif cups packaging/cups.list

.PHONEY:	dist
dist:	all
	$(RM) -r dist
	$(MAKE) $(MFLAGS) epm
	case `uname` in \
		*BSD*) $(MAKE) $(MFLAGS) bsd;; \
		Darwin*) $(MAKE) $(MFLAGS) osx;; \
		Linux*) test ! -x /usr/bin/rpm || $(MAKE) $(MFLAGS) rpm;; \
		SunOS*) $(MAKE) $(MFLAGS) pkg;; \
	esac


#
# Don't run top-level build targets in parallel...
#

.NOTPARALLEL:


#
# End of "$Id: Makefile 12414 2015-01-21 00:02:04Z msweet $".
#
