#
# "$Id$"
#
#   Datafile makefile for the Common UNIX Printing System (CUPS).
#
#   Copyright 2007-2008 by Apple Inc.
#   Copyright 1993-2006 by Easy Software Products.
#
#   These coded instructions, statements, and computer programs are the
#   property of Apple Inc. and are protected by Federal copyright
#   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
#   which should have been included with this file.  If this file is
#   file is missing or damaged, see the license at "http://www.cups.org/".
#

include ../Makedefs

#
# Data files...
#

BANNERS	=	\
		classified \
		confidential \
		secret \
		standard \
		topsecret \
		unclassified

CHARMAPS =	\
		euc-cn.txt \
		euc-jp.txt \
		euc-kr.txt \
		euc-tw.txt \
		iso-8859-10.txt \
		iso-8859-11.txt \
		iso-8859-13.txt \
		iso-8859-14.txt \
		iso-8859-15.txt \
		iso-8859-16.txt \
		iso-8859-1.txt \
		iso-8859-2.txt \
		iso-8859-3.txt \
		iso-8859-4.txt \
		iso-8859-5.txt \
		iso-8859-6.txt \
		iso-8859-7.txt \
		iso-8859-8.txt \
		iso-8859-9.txt \
		koi8-r.txt \
		koi8-u.txt \
		mac-roman.txt \
		windows-1250.txt \
		windows-1251.txt \
		windows-1252.txt \
		windows-1253.txt \
		windows-1254.txt \
		windows-1255.txt \
		windows-1256.txt \
		windows-1257.txt \
		windows-1258.txt \
		windows-1361.txt \
		windows-874.txt \
		windows-932.txt \
		windows-936.txt \
		windows-949.txt \
		windows-950.txt

CHARSETS =	\
		utf-8

DATAFILES =	\
		HPGLprolog \
		psglyphs \
		testprint.ps

PPDCFILES =	\
		epson.h \
		escp.h \
		font.defs \
		hp.h \
		label.h \
		media.defs \
		pcl.h \
		raster.defs


#
# Make everything...
#

all:


#
# Clean all config and object files...
#

clean:


#
# Dummy depend...
#

depend:


#
# Install files...
#

install:	all
	$(INSTALL_DIR) -m 755 $(DATADIR)/banners
	for file in $(BANNERS); do \
		$(INSTALL_DATA) $$file $(DATADIR)/banners; \
	done
	$(INSTALL_DIR) -m 755 $(DATADIR)/charmaps
	for file in $(CHARMAPS); do \
		$(INSTALL_DATA) $$file $(DATADIR)/charmaps; \
	done
	$(INSTALL_DIR) -m 755 $(DATADIR)/charsets
	for file in $(CHARSETS); do \
		$(INSTALL_DATA) $$file $(DATADIR)/charsets; \
	done
	$(INSTALL_DIR) -m 755 $(DATADIR)/data
	for file in $(DATAFILES); do \
		$(INSTALL_DATA) $$file $(DATADIR)/data; \
	done
	$(INSTALL_DIR) -m 755 $(DATADIR)/ppdc
	for file in $(PPDCFILES); do \
		$(INSTALL_DATA) $$file $(DATADIR)/ppdc; \
	done
	$(INSTALL_DIR) -m 755 $(DATADIR)/profiles


#
# Uninstall files...
#

uninstall:
	for file in $(BANNERS); do \
		$(RM) $(DATADIR)/banners/$$file; \
	done
	for file in $(CHARMAPS); do \
		$(RM) $(DATADIR)/charmaps/$$file; \
	done
	for file in $(CHARSETS); do \
		$(RM) $(DATADIR)/charsets/$$file; \
	done
	for file in $(DATAFILES); do \
		$(RM) $(DATADIR)/data/$$file; \
	done
	for file in $(PPDCFILES); do \
		$(RM) $(DATADIR)/ppdc/$$file; \
	done
	-$(RMDIR) $(DATADIR)/profiles
	-$(RMDIR) $(DATADIR)/ppdc
	-$(RMDIR) $(DATADIR)/data
	-$(RMDIR) $(DATADIR)/charsets
	-$(RMDIR) $(DATADIR)/charmaps
	-$(RMDIR) $(DATADIR)/banners
	-$(RMDIR) $(DATADIR)


#
# End of "$Id$".
#
