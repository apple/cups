#
# Makefile for CUPS documentation.
#

HTMLOPTIONS	=	--logo cups-bar.gif --numbered -t html
PDFOPTIONS	=	--duplex --logo cups-large.gif --numbered -t pdf
PSOPTIONS	=	--duplex --logo cups-large.gif --numbered -t ps2

.SUFFIXES:	.html .pdf .ps
.html.pdf:
	htmldoc $(PDFOPTIONS) -f $@ $<
.html.ps:
	htmldoc $(PSOPTIONS) -f $@ $<

DOCUMENTS	=	cmp.html idd.html sam.html sdd-berkeley.html \
			sdd-driver.html sdd-filter.html sdd-gui.html \
			sdd-posix.html sdd-scheduler.html sdd-systemv.html \
			ssr.html stp.html sum.html svd.html

all:	$(DOCUMENTS:.html=.pdf) $(DOCUMENTS:.html=.ps) $(DOCUMENTS:.html=.d) \
	overview.pdf overview.ps

cmp.d:	cmp.html
	rm -f cmp.d/*
	htmldoc $(HTMLOPTIONS) -d cmp.d cmp.html
idd.d:	idd.html
	rm -f idd.d/*
	htmldoc $(HTMLOPTIONS) -d idd.d idd.html
sam.d:	sam.html
	rm -f sam.d/*
	htmldoc $(HTMLOPTIONS) -d sam.d sam.html
sdd.d:	sdd.html
	rm -f sdd.d/*
	htmldoc $(HTMLOPTIONS) -d sdd.d sdd.html
ssr.d:	ssr.html
	rm -f ssr.d/*
	htmldoc $(HTMLOPTIONS) -d ssr.d ssr.html
stp.d:	stp.html
	rm -f stp.d/*
	htmldoc $(HTMLOPTIONS) -d stp.d stp.html
sum.d:	sum.html
	rm -f sum.d/*
	htmldoc $(HTMLOPTIONS) -d sum.d sum.html
svd.d:	svd.html
	rm -f svd.d/*
	htmldoc $(HTMLOPTIONS) -d svd.d svd.html

overview.ps overview.pdf:	overview.html
	htmldoc --webpage -f $@ overview.html

#
# End of Makefile.
#
