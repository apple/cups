#
# "$Id: Makefile,v 1.8 1999/05/18 21:21:43 mike Exp $"
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

.SUFFIXES:	.html .pdf .shtml
.shtml.html:
	htmldoc --title images/cups-large.gif --numbered -f $@ $<
.shtml.pdf:
	htmldoc --title images/cups-large.gif --duplex --compression=9 \
		--numbered --jpeg -f $@ $<

#
# Document files...
#

DOCUMENTS	=	cmp.shtml idd.shtml sam.shtml sdd.shtml ssr.shtml \
			stp.shtml sum.shtml svd.shtml
DOCIMAGES	=	images/navbar.gif
WEBPAGES	=	cups.css cupsdoc.css index.html documentation.html
WEBIMAGES	=	images/navbar.gif images/printer-idle.gif \
			images/printer-processing.gif \
			images/printer-stopped.gif

#
#
# Make all documents...
#

all:	$(DOCUMENTS:.shtml=.pdf) $(DOCUMENTS:.shtml=.html) overview.pdf

#
# Remove all generated files...
#

clean:
	$(RM) $(DOCUMENTS:.shtml=.pdf)
	$(RM) $(DOCUMENTS:.shtml=.html)
	$(RM) overview.pdf

#
# Install all documentation files...
#

install:
	-$(MKDIR) $(DATADIR)/doc
	$(CP) $(WEBPAGES) $(DATADIR)/doc
	$(CP) overview.html overview.pdf $(DATADIR)/doc
	$(CP) $(DOCUMENTS:.shtml=.html) $(DATADIR)/doc
	$(CP) $(DOCUMENTS:.shtml=.pdf) $(DATADIR)/doc
	-$(MKDIR) $(DATADIR)/doc/images
	$(CP) $(WEBIMAGES) $(DATADIR)/doc/images
	$(CP) $(DOCIMAGES) $(DATADIR)/doc/images

#
# The overview, admin guide, and user's guide get special attention...
#

overview.pdf:	overview.html
	htmldoc --duplex --compression=9 --jpeg --webpage -f $@ $<

sam.html:	sam.shtml
	htmldoc --title images/cups-large.gif -f $@ $<
sam.pdf:	sam.shtml
	htmldoc --title images/cups-large.gif --duplex --compression=9 \
		--jpeg -f $@ $<

sum.html:	sum.shtml
	htmldoc --title images/cups-large.gif -f $@ $<
sum.pdf:	sum.shtml
	htmldoc --title images/cups-large.gif --duplex --compression=9 \
		--jpeg -f $@ $<

#
# End of Makefile.
#
