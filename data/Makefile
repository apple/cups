#
# "$Id: Makefile,v 1.7 2000/03/30 05:19:21 mike Exp $"
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

CHARSETS =	cp874 \
		cp1250 \
		cp1251 \
		cp1252 \
		cp1253 \
		cp1254 \
		cp1255 \
		cp1256 \
		cp1257 \
		cp1258 \
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
	-$(MKDIR) $(DATADIR)
	-$(MKDIR) $(DATADIR)/data
	$(INSTALL_DATA) $(DATAFILES) $(DATADIR)/data
	-$(MKDIR) $(DATADIR)/charsets
	$(INSTALL_DATA) $(CHARSETS) $(DATADIR)/charsets


#
# End of "$Id: Makefile,v 1.7 2000/03/30 05:19:21 mike Exp $".
#
