#
# "$Id$"
#
#   Locale file makefile for the Common UNIX Printing System (CUPS).
#
#   Copyright 2007 by Apple Inc.
#   Copyright 1993-2007 by Easy Software Products.
#
#   These coded instructions, statements, and computer programs are the
#   property of Apple Inc. and are protected by Federal copyright
#   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
#   which should have been included with this file.  If this file is
#   file is missing or damaged, see the license at "http://www.cups.org/".
#

include ../Makedefs


OBJS	=	po2strings.o strings2po.o translate.o
TARGETS	=	po2strings strings2po translate


#
# Make everything...
#

all:	$(TARGETS)


#
# Clean all config and object files...
#

clean:
	$(RM) $(TARGETS) $(OBJS)


#
# Update dependencies (without system header dependencies...)
#

depend:
	makedepend -Y -I.. -fDependencies $(OBJS:.o=.c) >/dev/null 2>&1


#
# Install files...
#

install:	all $(INSTALL_LANGUAGES)

install-languages:
	$(INSTALL_DIR) -m 755 $(LOCALEDIR)
	for loc in $(LANGUAGES) ; do \
		if test -f cups_$$loc.po; then \
			$(INSTALL_DIR) -m 755 $(LOCALEDIR)/$$loc ; \
			$(INSTALL_DATA) cups_$$loc.po $(LOCALEDIR)/$$loc/cups_$$loc.po ; \
		fi ; \
	done


#
# Uninstall files...
#

uninstall: $(UNINSTALL_LANGUAGES)

uninstall-languages:
	-for loc in $(LANGUAGES) ; do \
		$(RM) $(LOCALEDIR)/$$loc/cups_$$loc.po ; \
	done


#
# pot - Creates/updates the cups.pot template file, and merges changes
#       into existing message catalogs.
#

pot:
	echo Updating cups.pot...
	mv cups.pot cups.pot.bck
	touch cups.pot
	cd ..; xgettext -o locale/cups.pot -j -s --keyword=_ --no-wrap \
		--copyright-holder="Easy Software Products" \
		--msgid-bugs-address="http://www.cups.org/str.php" \
		*/*.c
	(cat cups.header; \
	 cat cups.pot | sed -e '1,6d' -e '1,$$s/PACKAGE VERSION/CUPS 1.3/' \
	     -e '1,$$s/charset=CHARSET/charset=utf-8/'; \
	 cat cups.footer) > cups.pot.N
	mv cups.pot.N cups.pot
	for loc in $(LANGUAGES) ; do \
		echo Merging changes into cups_$$loc.po... ; \
		msgmerge -o cups_$$loc.po -s --no-location cups_$$loc.po cups.pot ; \
	done


#
# po2strings - A simple utility which uses iconv to convert GNU gettext
#              message catalogs to Mac OS X .strings files.
#
# po2strings filename.po filename.strings
#

po2strings:	po2strings.o ../cups/$(LIBCUPS)
	echo Linking $<...
	$(CC) $(LDFLAGS) -o po2strings po2strings.o $(LIBS)


#
# strings2po - A simple utility which uses iconv to convert Mac OS X
#              .strings files to GNU gettext message catalogs.
#
# strings2po filename.strings filename.po
#

strings2po:	strings2po.o
	echo Linking $<...
	$(CC) $(LDFLAGS) -o strings2po strings2po.o


#
# translate - A simple utility which uses Google to translate the cups.pot
#             file to one of several languages.
#
# translate outfile language
#

translate:	translate.o ../cups/$(LIBCUPS)
	echo Linking $<...
	$(CC) $(LDFLAGS) -o translate translate.o $(LIBS)


#
# Dependencies...
#
  
include Dependencies


#
# End of "$Id$".
#
