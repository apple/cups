#
# Makefile for CUPS documentation.
#

COMMON		=	--bodyimage draft.gif --logo cups-bar.gif --numbered
HTMLOPTIONS	=	$(COMMON) -t html
PDFOPTIONS	=	$(COMMON) --duplex --compression=9 -t pdf
PSOPTIONS	=	$(COMMON) --duplex -t ps2

.SUFFIXES:	.html .pdf .ps
.html.pdf:
	htmldoc $(PDFOPTIONS) -f $@ $<
.html.ps:
	htmldoc $(PSOPTIONS) -f $@ $<

DOCUMENTS	=	cmp.html idd.html sam.html sdd-berkeley.html \
			sdd-backend.html sdd-filter.html sdd-gui.html \
			sdd-posix.html sdd-scheduler.html sdd-systemv.html \
			ssdd.html ssr.html stp.html sum.html svd.html

all:	$(DOCUMENTS:.html=.pdf) $(DOCUMENTS:.html=.ps) \
	overview.pdf overview.ps

overview.ps:	overview.html
	htmldoc $(PSOPTIONS) --webpage -f $@ overview.html

overview.pdf:	overview.html
	htmldoc $(PDFOPTIONS) --webpage -f $@ overview.html

#
# End of Makefile.
#
