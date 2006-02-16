#
# "$Id$"
#
#   Locale file makefile for the Common UNIX Printing System (CUPS).
#
#   Copyright 1993-2006 by Easy Software Products.
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
#       Hollywood, Maryland 20636 USA
#
#       Voice: (301) 373-9600
#       EMail: cups-info@cups.org
#         WWW: http://www.cups.org
#

include ../Makedefs

#
# Locales...
#

LOCALES	=	fr ja


#
# Make everything...
#

all:	translate


#
# Clean all config and object files...
#

clean:
	$(RM) translate translate.o


#
# Update dependencies...
#

depend:


#
# Install files...
#

install:	all
	$(INSTALL_DIR) -m 755 $(LOCALEDIR)
	for loc in $(LOCALES) ; do \
		$(INSTALL_DIR) -m 755 $(LOCALEDIR)/$$loc ; \
		$(INSTALL_DATA) cups_$$loc.po $(LOCALEDIR)/$$loc/cups_$$loc.po ; \
	done


#
# pot - Creates/updates the cups.pot template file, and merges changes
#       into existing message catalogs.
#

pot:
	echo Updating cups.pot...
	cd ..; xgettext -o locale/cups.pot -j --keyword=_ --no-wrap \
		--copyright-holder="Easy Software Products" \
		--msgid-bugs-address="http://www.cups.org/str.php" \
		*/*.c
	(cat cups.header; \
	 tail +6 cups.pot | sed -e '1,$$s/PACKAGE VERSION/CUPS 1.2/' \
	     -e '1,$$s/charset=CHARSET/charset=utf-8/'; \
	 cat cups.footer) > cups.pot.N
	mv cups.pot.N cups.pot
	for loc in $(LOCALES) ; do \
		echo Merging changes into cups_$$loc.po... ; \
		msgmerge -o cups_$$loc.po cups_$$loc.po cups.pot ; \
	done


#
# translate - A simple utility which uses Google to translate the cups.pot
#             file to one of several languages.
#
# translate outfile language
#

translate:	translate.o ../cups/$(LIBCUPS)
	echo Linking $<...
	$(CC) $(LDFLAGS) -o translate translate.o $(LIBS)

translate.o:	../cups/http.h ../cups/i18n.h ../cups/language.h ../cups/string.h


#
# End of "$Id$".
#
