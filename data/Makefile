#
# "$Id: Makefile,v 1.10 2000/07/07 17:02:38 mike Exp $"
#
#   Datafile makefile for the Common UNIX Printing System (CUPS).
#
#   Copyright 1993-2000 by Easy Software Products.
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

include ../Makedefs

#
# Data files...
#

BANNERS	=	classified \
		confidential \
		secret \
		standard \
		topsecret \
		unclassified

CHARSETS =	windows-874 \
		windows-1250 \
		windows-1251 \
		windows-1252 \
		windows-1253 \
		windows-1254 \
		windows-1255 \
		windows-1256 \
		windows-1257 \
		windows-1258 \
		iso-8859-1 \
		iso-8859-2 \
		iso-8859-3 \
		iso-8859-4 \
		iso-8859-5 \
		iso-8859-6 \
		iso-8859-7 \
		iso-8859-8 \
		iso-8859-9 \
		iso-8859-10 \
		iso-8859-13 \
		iso-8859-14 \
		iso-8859-15 \
		utf-8
DATAFILES =	HPGLprolog psglyphs testprint.ps


#
# Make everything...
#

all:


#
# Clean all config and object files...
#

clean:


#
# Install files...
#

install:
	-$(MKDIR) $(DATADIR)/banners
	$(CHMOD) ugo+rx $(DATADIR)
	$(CHMOD) ugo+rx $(DATADIR)/banners
	$(INSTALL_DATA) $(BANNERS) $(DATADIR)/banners
	-$(MKDIR) $(DATADIR)/charsets
	$(CHMOD) ugo+rx $(DATADIR)/charsets
	$(INSTALL_DATA) $(CHARSETS) $(DATADIR)/charsets
	-$(MKDIR) $(DATADIR)/data
	$(CHMOD) ugo+rx $(DATADIR)/data
	$(INSTALL_DATA) $(DATAFILES) $(DATADIR)/data
	-if test "$(PAMDIR)" != ""; then \
		$(MKDIR) $(PAMDIR); \
		$(CHMOD) ugo+rx $(PAMDIR); \
		$(INSTALL_DATA) cups.pam $(PAMDIR)/cups; \
	fi


#
# End of "$Id: Makefile,v 1.10 2000/07/07 17:02:38 mike Exp $".
#
