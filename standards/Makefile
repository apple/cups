#
# "$Id: Makefile 7871 2008-08-27 21:12:43Z mike $"
#
#   Standards makefile for CUPS.
#
#   Copyright 2007-2010 by Apple Inc.
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
		rfc1155.txt \
		rfc1157.txt \
		rfc1179.txt \
		rfc1213.txt \
		rfc1321.txt \
		rfc2222.txt \
		rfc2246.txt \
		rfc2487.txt \
		rfc2554.txt \
		rfc2567.txt \
		rfc2568.txt \
		rfc2569.txt \
		rfc2578.txt \
		rfc2595.txt \
		rfc2616.txt \
		rfc2617.txt \
		rfc2712.txt \
		rfc2790.txt \
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
		rfc3805.txt \
		rfc3875.txt \
		rfc3986.txt \
		rfc3995.txt \
		rfc3996.txt \
		rfc3997.txt \
		rfc3998.txt \
		rfc4122.txt \
		rfc4234.txt

.SUFFIXES:	.html .txt
.txt.html:	rfctohtml
	echo Converting $< to HTML...
	./rfctohtml $< ../doc/help/$@


#
# Make everything...
#

all:	rfctohtml $(RFCS:.txt=.html)


#
# Make library targets...
#

libs:


#
# Make unit tests...
#

unittests:


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
# Install all targets...
#

install:	all install-data install-headers install-libs install-exec


#
# Install data files...
#

install-data:


#
# Install programs...
#

install-exec:


#
# Install headers...
#

install-headers:


#
# Install libraries...
#

install-libs:


#
# Uninstall files...
#

uninstall:


#
# rfctohtml - make html versions of RFCs...
#

rfctohtml:	rfctohtml.o ../cups/$(LIBCUPSSTATIC)
	$(CC) $(ARCHFLAGS) $(LDFLAGS) -o $@ rfctohtml.o ../cups/$(LIBCUPSSTATIC) \
		$(LIBGSSAPI) $(SSLLIBS) $(DNSSDLIBS) $(COMMONLIBS) $(LIBZ)


#
# End of "$Id: Makefile 7871 2008-08-27 21:12:43Z mike $".
#
