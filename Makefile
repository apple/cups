#
# "$Id: Makefile,v 1.33 2001/06/27 21:53:06 mike Exp $"
#
#   Top-level Makefile for the Common UNIX Printing System (CUPS).
#
#   Copyright 1997-2001 by Easy Software Products, all rights reserved.
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
#       Hollywood, Maryland 20636-3111 USA
#
#       Voice: (301) 373-9603
#       EMail: cups-info@cups.org
#         WWW: http://www.cups.org
#

include Makedefs

#
# Directories to make...
#

DIRS	=	cups backend berkeley cgi-bin filter man pdftops pstoraster \
		scheduler systemv

#
# Make all targets...
#

all:
	for dir in $(DIRS); do\
		echo Making all in $$dir... ;\
		(cd $$dir ; $(MAKE) $(MFLAGS)) || exit 1;\
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
# Install object and target files...
#

install:
	for dir in $(DIRS); do\
		echo Installing in $$dir... ;\
		(cd $$dir; $(MAKE) $(MFLAGS) install) || exit 1;\
	done
	echo Installing in conf...
	(cd conf; $(MAKE) $(MFLAGS) install)
	echo Installing in data...
	(cd data; $(MAKE) $(MFLAGS) install)
	echo Installing in doc...
	(cd doc; $(MAKE) $(MFLAGS) install)
	echo Installing in fonts...
	(cd fonts; $(MAKE) $(MFLAGS) install)
	echo Installing in locale...
	(cd locale; $(MAKE) $(MFLAGS) install)
	echo Installing in ppd...
	(cd ppd; $(MAKE) $(MFLAGS) install)
	echo Installing in templates...
	(cd templates; $(MAKE) $(MFLAGS) install)
	echo Installing startup script...
	if test "x$INITDIR" != "x"; then \
		$(INSTALL_DIR) $(prefix)/$(INITDIR)/init.d; \
		$(INSTALL_SCRIPT) cups.sh $(prefix)/$(INITDIR)/init.d/cups; \
		$(INSTALL_DIR) $(prefix)/$(INITDIR)/rc0.d; \
		$(INSTALL_SCRIPT) cups.sh  $(prefix)/$(INITDIR)/rc0.d/K00cups; \
		$(INSTALL_DIR) $(prefix)/$(INITDIR)/rc2.d; \
		$(INSTALL_SCRIPT) cups.sh $(prefix)/$(INITDIR)/rc2.d/S99cups; \
		$(INSTALL_DIR) $(prefix)/$(INITDIR)/rc3.d; \
		$(INSTALL_SCRIPT) cups.sh $(prefix)/$(INITDIR)/rc3.d/S99cups; \
		$(INSTALL_DIR) $(prefix)/$(INITDIR)/rc5.d; \
		$(INSTALL_SCRIPT) cups.sh $(prefix)/$(INITDIR)/rc5.d/S99cups; \
	fi
	if test "x$(INITDIR)" = "x" -a "x$(INITDDIR)" != "x"; then \
		$(INSTALL_DIR) $(prefix)/$(INITDDIR); \
		$(INSTALL_SCRIPT) cups.sh $(prefix)/$(INITDDIR)/cups; \
	fi


#
# Run the test suite...
#

test:	all
	echo Running CUPS test suite...
	cd test; ./run-stp-tests.sh


#
# Make software distributions using EPM (http://www.easysw.com/epm)...
#

EPMFLAGS	=	-v \
			AMANDIR=$(AMANDIR) \
			BINDIR=$(BINDIR) DATADIR=$(DATADIR) \
			DOCDIR=$(DOCDIR) INCLUDEDIR=$(INCLUDEDIR) \
			LIBDIR=$(LIBDIR) LOCALEDIR=$(LOCALEDIR) \
			LOGDIR=$(LOGDIR) MANDIR=$(MANDIR) \
			PAMDIR=$(PAMDIR) REQUESTS=$(REQUESTS) \
			SBINDIR=$(SBINDIR) SERVERBIN=$(SERVERBIN) \
			SERVERROOT=$(SERVERROOT)

aix:
	epm $(EPMFLAGS) -f aix cups

bsd:
	epm $(EPMFLAGS) -f bsd cups

epm:
	epm $(EPMFLAGS) cups

rpm:
	epm $(EPMFLAGS) -f rpm cups

deb:
	epm $(EPMFLAGS) -f deb cups

depot:
	epm $(EPMFLAGS) -f depot cups

pkg:
	epm $(EPMFLAGS) -f pkg cups

tardist:
	epm $(EPMFLAGS) -f tardist cups

#
# End of "$Id: Makefile,v 1.33 2001/06/27 21:53:06 mike Exp $".
#
