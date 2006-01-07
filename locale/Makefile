#
# "$Id$"
#
#   Locale file makefile for the Common UNIX Printing System (CUPS).
#
#   Copyright 1993-2005 by Easy Software Products.
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

LOCALES	=


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
	$(INSTALL_DIR) $(LOCALEDIR)
#	for loc in $(LOCALES) ; do \
#		$(INSTALL_DIR) $(LOCALEDIR)/$$loc ; \
#		$(INSTALL_DATA) cups_$$loc.po $(LOCALEDIR)/$$loc ; \
#	done


#
# pot - Creates/updates the cups.pot template file, and merges changes
#       into existing message catalogs.
#

pot:
	echo Updating cups.pot...
	cd ..; xgettext -o locale/cups.pot -j --keyword=_ \
		--copyright-holder="Easy Software Products" \
		--msgid-bugs-address="http://www.cups.org/str.php" \
		*/*.c
	(cat cups.header; tail +6 cups.pot; cat cups.footer) > cups.pot.N
	mv cups.pot.N cups.pot
#	for loc in $(LOCALES) ; do \
#		echo Merging changes into cups_$$loc.po... ; \
#		msgmerge cups_$$loc.po cups.pot ; \
#	done


#
# translate - a simple utility to use Bablefish to translate the message
#             file to one of several languages.
#
# translate outfile language
#

translate:	translate.o ../cups/$(LIBCUPS)
	echo Linking $<...
	$(CC) $(LDFLAGS) -o translate translate.o $(LIBS)

translate.o:	../cups/http.h


#
# End of "$Id$".
#
