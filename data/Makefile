#
# "$Id: Makefile,v 1.12.2.6 2002/09/26 16:58:49 mike Exp $"
#
#   Datafile makefile for the Common UNIX Printing System (CUPS).
#
#   Copyright 1993-2002 by Easy Software Products.
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
		i18n_sdd.txt \
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
		unibreak.txt \
		uni-comb.txt \
		uni-fold.txt \
		uni-full.txt \
		uni-line.txt \
		uni-nfc.txt \
		uni-nfd.txt \
		uni-nfkd.txt \
		uni-prop.txt \
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
		windows-874 \
		windows-1250 \
		windows-1251 \
		windows-1252 \
		windows-1253 \
		windows-1254 \
		windows-1255 \
		windows-1256 \
		windows-1257 \
		windows-1258 \
		koi8-r \
		koi8-u \
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

DATAFILES =	\
		HPGLprolog \
		psglyphs \
		testprint.ps


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
	$(INSTALL_DIR) $(DATADIR)/banners
	for file in $(BANNERS); do \
		$(INSTALL_DATA) $$file $(DATADIR)/banners; \
	done
	$(INSTALL_DIR) $(DATADIR)/charmaps
	for file in $(CHARMAPS); do \
		$(INSTALL_DATA) $$file $(DATADIR)/charmaps; \
	done
	$(INSTALL_DIR) $(DATADIR)/charsets
	for file in $(CHARSETS); do \
		$(INSTALL_DATA) $$file $(DATADIR)/charsets; \
	done
	$(INSTALL_DIR) $(DATADIR)/data
	for file in $(DATAFILES); do \
		$(INSTALL_DATA) $$file $(DATADIR)/data; \
	done


#
# End of "$Id: Makefile,v 1.12.2.6 2002/09/26 16:58:49 mike Exp $".
#
