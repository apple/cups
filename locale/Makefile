#
# "$Id: Makefile,v 1.6 2000/04/20 19:56:38 mike Exp $"
#
#   Locale file makefile for the Common UNIX Printing System (CUPS).
#
#   Copyright 1993-1999 by Easy Software Products.
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
# Locales...
#

LOCALES	=	C de en es fr it


#
# Make everything...
#

all:	translate


#
# Clean all config and object files...
#

clean:


#
# Install files...
#

install:
	-$(MKDIR) $(LIBDIR)/locale
	for dir in $(LOCALES) ; do \
		if test ! -d $(LOCALEDIR)/$$dir ; then \
			$(MKDIR) $(LOCALEDIR)/$$dir ; \
		fi ; \
		$(INSTALL_DATA) $$dir/cups_$$dir $(LOCALEDIR)/$$dir ; \
	done


#
# translate - a simple utility to use Bablefish to translate the POSIX message
#             file to one of several languages.
#
# translate outfile language
#

translate:	translate.o ../cups/$(LIBCUPS)
	echo Linking $<...
	$(CC) $(LDFLAGS) -o translate translate.o $(LIBS)

translate.o:	../cups/http.h


#
# End of "$Id: Makefile,v 1.6 2000/04/20 19:56:38 mike Exp $".
#
