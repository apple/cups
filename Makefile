#
# "$Id: Makefile,v 1.22 2000/07/31 16:12:11 mike Exp $"
#
#   Top-level Makefile for the Common UNIX Printing System (CUPS).
#
#   Copyright 1997-2000 by Easy Software Products, all rights reserved.
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
	echo Installing in templates...
	(cd templates; $(MAKE) $(MFLAGS) install)
	echo Installing startup script...
	if test -d /sbin/init.d; then \
		$(INSTALL_SCRIPT) cups.sh /sbin/init.d/cups; \
		$(CHMOD) ugo+rx /sbin/init.d/cups; \
		ln -s ../init.d/cups /sbin/rc0.d/K000cups; \
		ln -s ../init.d/cups /sbin/rc2.d/S999cups; \
	fi
	if test -d /etc/rc.d/init.d; then \
		$(INSTALL_SCRIPT) cups.sh /etc/rc.d/init.d/cups; \
		$(CHMOD) ugo+rx /etc/rc.d/cups; \
		ln -s ../init.d/cups /etc/rc.d/rc0.d/K00cups; \
		ln -s ../init.d/cups /etc/rc.d/rc2.d/S99cups; \
	fi
	if test -d /etc/init.d; then \
		$(INSTALL_SCRIPT) cups.sh /etc/init.d/cups; \
		$(CHMOD) ugo+rx /etc/init.d/cups; \
		ln -s ../init.d/cups /etc/rc0.d/K00cups; \
		ln -s ../init.d/cups /etc/rc2.d/S99cups; \
	fi

#
# Make software distributions using EPM (http://www.easysw.com/epm)...
#

EPMFLAGS	=	-v \
			BINDIR=$(BINDIR) DATADIR=$(DATADIR) \
			DOCDIR=$(DOCDIR) ESP_ROOT=$(ESP_ROOT) \
			INCLUDEDIR=$(INCLUDEDIR) LIBDIR=$(LIBDIR) \
			LOCALEDIR=$(LOCALEDIR) LOGDIR=$(LOGDIR) \
			MANDIR=$(MANDIR) PAMDIR=$(PAMDIR) \
			REQUESTS=$(REQUESTS) SBINDIR=$(SBINDIR) \
			SERVERBIN=$(SERVERBIN) SERVERROOT=$(SERVERROOT)

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
# End of "$Id: Makefile,v 1.22 2000/07/31 16:12:11 mike Exp $".
#
