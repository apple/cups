#
# "$Id: Makefile,v 1.25 2000/05/22 18:45:26 mike Exp $"
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

.SUFFIXES:	.html .pdf .ps .shtml
.shtml.html:
	echo Formatting $@...
	htmldoc --titleimage images/cups-large.gif --numbered -f $@ $<
.shtml.pdf:
	echo Formatting $@...
	htmldoc --titleimage images/cups-large.gif --duplex --compression=9 \
		--numbered --jpeg -f $@ $<
.shtml.ps:
	echo Formatting $@...
	htmldoc --titleimage images/cups-large.gif --duplex --numbered \
		--jpeg -f $@ $<


#
# Document files...
#

DOCUMENTS	=	cmp.shtml idd.shtml ipp.shtml sam.shtml sdd.shtml \
			spm.shtml ssr.shtml stp.shtml sum.shtml svd.shtml
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
			images/config-printer.gif \
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
# Make all documents...
#

all:	$(DOCUMENTS:.shtml=.pdf) $(DOCUMENTS:.shtml=.html) overview.pdf


#
# Make PS files...
#

ps:	$(DOCUMENTS:.shtml=.ps) overview.ps


#
# Make manuals for printing...
#

print:	sam-7x8.pdf spm-7x8.pdf sum-7x8.pdf \
	sam-7x8.ps spm-7x8.ps sum-7x8.ps


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
	$(INSTALL_MAN) $(WEBPAGES) $(DOCDIR)
	$(INSTALL_MAN) overview.html overview.pdf $(DOCDIR)
	$(INSTALL_MAN) $(DOCUMENTS:.shtml=.html) $(DOCDIR)
	$(INSTALL_MAN) $(DOCUMENTS:.shtml=.pdf) $(DOCDIR)
	-$(MKDIR) $(DOCDIR)/images
	$(INSTALL_MAN) $(WEBIMAGES) $(DOCDIR)/images
	$(INSTALL_MAN) $(DOCIMAGES) $(DOCDIR)/images


#
# The overview, admin manual, programmers manual, and users manual get
# special attention...
#

overview.pdf:	overview.html
	echo Formatting $@...
	htmldoc --duplex --compression=9 --jpeg --webpage -f overview.pdf overview.html
overview.ps:	overview.html
	echo Formatting $@...
	htmldoc --duplex --jpeg --webpage -f overview.ps overview.html

sam.html:	sam.shtml
	echo Formatting $@...
	htmldoc --titleimage images/cups-large.gif -f $@ $<
sam.pdf:	sam.shtml
	echo Formatting $@...
	htmldoc --titleimage images/cups-large.gif --duplex --compression=9 \
		--jpeg -f $@ $<
sam.ps:		sam.shtml
	echo Formatting $@...
	htmldoc --titleimage images/cups-large.gif --duplex --jpeg -f $@ $<

spm.html:	spm.shtml
	echo Formatting $@...
	htmldoc --titleimage images/cups-large.gif -f $@ $<
spm.pdf:	spm.shtml
	echo Formatting $@...
	htmldoc --titleimage images/cups-large.gif --duplex --compression=9 \
		--jpeg -f $@ $<
spm.ps:		spm.shtml
	echo Formatting $@...
	htmldoc --titleimage images/cups-large.gif --duplex --jpeg -f $@ $<

sum.html:	sum.shtml
	echo Formatting $@...
	htmldoc --titleimage images/cups-large.gif -f $@ $<
sum.pdf:	sum.shtml
	echo Formatting $@...
	htmldoc --titleimage images/cups-large.gif --duplex --compression=9 \
		--jpeg -f $@ $<
sum.ps:		sum.shtml
	echo Formatting $@...
	htmldoc --titleimage images/cups-large.gif --duplex --jpeg -f $@ $<

sam-7x8.pdf:	sam.shtml
	echo Formatting $@...
	htmldoc --titleimage images/cups-large.gif --duplex --compression=9 \
		--pagelayout tworight --pagemode document \
		--jpeg --size 7x8.5in --left 0.5in --right 0.25in \
		--top 0.25in --bottom 0.25in --fontsize 10 --headfootsize 10 -f $@ sam.shtml
sam-7x8.ps:		sam.shtml
	echo Formatting $@...
	htmldoc --titleimage images/cups-large.gif --duplex --jpeg \
		--size 7x8.5in --left 0.5in --right 0.25in \
		--top 0.25in --bottom 0.25in --fontsize 10 --headfootsize 10 -f $@ sam.shtml
spm-7x8.pdf:	spm.shtml
	echo Formatting $@...
	htmldoc --titleimage images/cups-large.gif --duplex --compression=9 \
		--pagelayout tworight --pagemode document \
		--jpeg --size 7x8.5in --left 0.5in --right 0.25in \
		--top 0.25in --bottom 0.25in --fontsize 10 --headfootsize 10 -f $@ spm.shtml
spm-7x8.ps:		spm.shtml
	echo Formatting $@...
	htmldoc --titleimage images/cups-large.gif --duplex --jpeg \
		--size 7x8.5in --left 0.5in --right 0.25in \
		--top 0.25in --bottom 0.25in --fontsize 10 --headfootsize 10 -f $@ spm.shtml
sum-7x8.pdf:	sum.shtml
	echo Formatting $@...
	htmldoc --titleimage images/cups-large.gif --duplex --compression=9 \
		--pagelayout tworight --pagemode document \
		--jpeg --size 7x8.5in --left 0.5in --right 0.25in \
		--top 0.25in --bottom 0.25in --fontsize 10 --headfootsize 10 -f $@ sum.shtml
sum-7x8.ps:		sum.shtml
	echo Formatting $@...
	htmldoc --titleimage images/cups-large.gif --duplex --jpeg \
		--size 7x8.5in --left 0.5in --right 0.25in \
		--top 0.25in --bottom 0.25in --fontsize 10 --headfootsize 10 -f $@ sum.shtml

#
# End of Makefile.
#
