#
# "$Id: Makefile,v 1.4 1999/05/11 21:00:09 mike Exp $"
#
#   Documentation makefile for the Common UNIX Printing System (CUPS).
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
# HTMLDOC generation rules...
#

.SUFFIXES:	.html .pdf
.html.pdf:
	htmldoc --title images/cups-large.gif --duplex --compression=9 \
		--jpeg -f $@ $<

#
# Document files...
#

DOCUMENTS	=	cmp.html idd.html overview.html \
			sam.html sdd.html ssr.html stp.html sum.html svd.html
DOCIMAGES	=	images/navbar.gif
WEBPAGES	=	cups.css cupsdoc.css index.html documentation.html
WEBIMAGES	=	images/navbar.gif images/printer-idle.gif \
			images/printer-processing.gif \
			images/printer-stopped.gif

#
#
# Make all documents...
#

all:	$(DOCUMENTS:.html=.pdf)

#
# Remove all generated files...
#

clean:
	$(RM) $(DOCUMENTS:.html=.pdf)

#
# Install all documentation files...
#

install:
	-$(MKDIR) $(SERVERROOT)/doc
	$(CP) $(WEBPAGES) $(SERVERROOT)/doc
	$(CP) $(DOCUMENTS) $(SERVERROOT)/doc
	$(CP) $(DOCUMENTS:.html=.pdf) $(SERVERROOT)/doc
	-$(MKDIR) $(SERVERROOT)/doc/images
	$(CP) $(WEBIMAGES) $(SERVERROOT)/doc/images
	$(CP) $(DOCIMAGES) $(SERVERROOT)/doc/images

#
# Make the overview whitepaper (special format).
#

overview.pdf:	overview.html
	htmldoc --duplex --compression=9 -jpeg --webpage -f $@ $<

#
# End of Makefile.
#
