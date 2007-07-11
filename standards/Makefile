#
# "$Id$"
#
#   Standards makefile for the Common UNIX Printing System (CUPS).
#
#   Copyright 2007 by Apple Inc.
#   Copyright 2006 by Easy Software Products.
#
#   These coded instructions, statements, and computer programs are the
#   property of Apple Inc. and are protected by Federal copyright
#   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
#   which should have been included with this file.  If this file is
#   file is missing or damaged, see the license at "http://www.cups.org/".
#

include ../Makedefs


#
# Standards...
#

RFCS	=	\
		rfc1179.txt \
		rfc1321.txt \
		rfc2222.txt \
		rfc2246.txt \
		rfc2487.txt \
		rfc2554.txt \
		rfc2567.txt \
		rfc2568.txt \
		rfc2569.txt \
		rfc2595.txt \
		rfc2616.txt \
		rfc2617.txt \
		rfc2712.txt \
		rfc2817.txt \
		rfc2818.txt \
		rfc2821.txt \
		rfc2822.txt \
		rfc2910.txt \
		rfc2911.txt \
		rfc2965.txt \
		rfc3196.txt \
		rfc3239.txt \
		rfc3380.txt \
		rfc3381.txt \
		rfc3382.txt \
		rfc3391.txt \
		rfc3510.txt \
		rfc3712.txt \
		rfc3986.txt \
		rfc3995.txt \
		rfc3996.txt \
		rfc3997.txt \
		rfc3998.txt

.SUFFIXES:	.html .txt
.txt.html:	rfctohtml
	echo Converting $< to HTML...
	./rfctohtml $< ../doc/help/$@


#
# Make everything...
#

all:	rfctohtml $(RFCS:.txt=.html)


#
# Clean all config and object files...
#

clean:
	$(RM) rfctohtml rfctohtml.o
	$(RM) $(RFCS:.txt=.html)


#
# Dummy depend target...
#

depend:


#
# Install files...
#

install:	all


#
# Uninstall files...
#

uninstall:


#
# rfctohtml - make html versions of RFCs...
#

rfctohtml:	rfctohtml.o ../cups/libcups.a
	$(CC) $(LDFLAGS) -o $@ rfctohtml.o ../cups/libcups.a \
		$(SSLLIBS) $(COMMONLIBS) $(LIBZ)


#
# End of "$Id$".
#
