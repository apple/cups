#
# "$Id: Makefile,v 1.17 2000/02/29 14:47:28 mike Exp $"
#
#   Documentation makefile for the Common UNIX Printing System (CUPS).
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
# HTMLDOC generation rules...
#

.SUFFIXES:	.html .pdf .shtml
.shtml.html:
	echo Formatting $@...
	htmldoc --titleimage images/cups-large.gif --numbered -f $@ $<
.shtml.pdf:
	echo Formatting $@...
	htmldoc --titleimage images/cups-large.gif --duplex --compression=9 \
		--numbered --jpeg -f $@ $<

#
# Document files...
#

DOCUMENTS	=	cmp.shtml idd.shtml sam.shtml sdd.shtml spm.shtml \
			ssr.shtml stp.shtml sum.shtml
DOCIMAGES	=	images/cups-block-diagram.gif images/cups-large.gif \
			images/cups-medium.gif images/cups-small.gif
WEBPAGES	=	cups.css cupsdoc.css index.html documentation.html
WEBIMAGES	=	images/accept-jobs.gif \
			images/add-class.gif \
			images/add-printer.gif \
			images/cancel-job.gif \
			images/cancel-jobs.gif \
			images/cancel.gif \
			images/classes.gif \
			images/continue.gif \
			images/delete-class.gif \
			images/delete-printer.gif \
			images/draft.gif \
			images/hold-job.gif \
			images/left.gif \
			images/logo.gif \
			images/manage-classes.gif \
			images/manage-jobs.gif \
			images/manage-printers.gif \
			images/modify-class.gif \
			images/modify-printer.gif \
			images/navbar.gif \
			images/print-test-page.gif \
			images/printer-idle.gif \
			images/printer-processing.gif \
			images/printer-stopped.gif \
			images/reject-jobs.gif \
			images/release-job.gif \
			images/right.gif \
			images/show-active.gif \
			images/show-completed.gif \
			images/start-class.gif \
			images/start-printer.gif \
			images/stop-class.gif \
			images/stop-printer.gif

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
	-$(MKDIR) $(DOCDIR)
	$(CP) $(WEBPAGES) $(DOCDIR)
	$(CP) overview.html overview.pdf $(DOCDIR)
	$(CP) $(DOCUMENTS:.shtml=.html) $(DOCDIR)
	$(CP) $(DOCUMENTS:.shtml=.pdf) $(DOCDIR)
	-$(MKDIR) $(DOCDIR)/images
	$(CP) $(WEBIMAGES) $(DOCDIR)/images
	$(CP) $(DOCIMAGES) $(DOCDIR)/images

#
# The overview, admin manual, programmers manual, and users manual get
# special attention...
#

overview.pdf:	overview.html
	echo Formatting $@...
	htmldoc --duplex --compression=9 --jpeg --webpage -f overview.pdf overview.html

sam.html:	sam.shtml
	echo Formatting $@...
	htmldoc --titleimage images/cups-large.gif -f $@ $<
sam.pdf:	sam.shtml
	echo Formatting $@...
	htmldoc --titleimage images/cups-large.gif --duplex --compression=9 \
		--jpeg -f $@ $<

spm.html:	spm.shtml
	echo Formatting $@...
	htmldoc --titleimage images/cups-large.gif -f $@ $<
spm.pdf:	spm.shtml
	echo Formatting $@...
	htmldoc --titleimage images/cups-large.gif --duplex --compression=9 \
		--jpeg -f $@ $<

sum.html:	sum.shtml
	echo Formatting $@...
	htmldoc --titleimage images/cups-large.gif -f $@ $<
sum.pdf:	sum.shtml
	echo Formatting $@...
	htmldoc --titleimage images/cups-large.gif --duplex --compression=9 \
		--jpeg -f $@ $<

#
# End of Makefile.
#
