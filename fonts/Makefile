#
# "$Id: Makefile,v 1.6 1999/05/10 17:42:00 mike Exp $"
#
#   Fonts makefile for the Common UNIX Printing System (CUPS).
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
# Font files...
#

FONTS	=	AvantGarde-Book AvantGarde-BookOblique AvantGarde-Demi \
		AvantGarde-DemiOblique Bookman-Demi Bookman-DemiItalic \
		Bookman-Light Bookman-LightItalic Courier Courier-Bold \
		Courier-BoldOblique Courier-Oblique Helvetica \
		Helvetica-Bold Helvetica-BoldOblique Helvetica-Narrow \
		Helvetica-Narrow-Bold Helvetica-Narrow-BoldOblique \
		Helvetica-Narrow-Oblique Helvetica-Oblique \
		NewCenturySchlbk-Bold NewCenturySchlbk-BoldItalic \
		NewCenturySchlbk-Italic NewCenturySchlbk-Roman \
		Palatino-Bold Palatino-BoldItalic Palatino-Italic \
		Palatino-Roman Symbol Times-Bold Times-BoldItalic \
		Times-Italic Times-Roman Utopia-Bold Utopia-BoldItalic \
		Utopia-Italic Utopia-Regular ZapfChancery-MediumItalic \
		ZapfDingbats

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
	-$(MKDIR) $(DATADIR)/fonts
	$(CP) $(FONTS) $(DATADIR)/fonts

#
# End of "$Id: Makefile,v 1.6 1999/05/10 17:42:00 mike Exp $".
#
