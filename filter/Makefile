#!/usr/sbin/smake
#
# "$Id: Makefile,v 1.2 1999/03/06 18:02:26 mike Exp $"
#
#    for espPrint, a collection of printer/image software.
#
#   Copyright (c) 1993-1995 by Easy Software Products
#
#   These coded instructions, statements, and computer  programs  contain
#   unpublished  proprietary  information  of Easy Software Products, and
#   are protected by Federal copyright law.  They may  not  be  disclosed
#   to  third  parties  or  copied or duplicated in any form, in whole or
#   in part, without the prior written consent of Easy Software Products.
#
# Revision History:
#
#   $Log: Makefile,v $
#   Revision 1.2  1999/03/06 18:02:26  mike
#   Updated for CVS check-in.
#
#   Revision 1.1  1996/08/24  19:41:24  mike
#   Initial revision
#
#

#
# Standard definitions...
#

include ../Makedefs.$(HOSTTYPE)


TARGETS	=	$(HOSTTYPE)/hpgl2ps

#
# Make all targets...
#

m_$(HOSTTYPE):	$(TARGETS)

#
# Clean all garbage...
#

clean:

#
# hpgl2ps
#

OBJS	=	$(HOSTTYPE)/attr.o $(HOSTTYPE)/char.o $(HOSTTYPE)/config.o \
		$(HOSTTYPE)/input.o $(HOSTTYPE)/main.o $(HOSTTYPE)/polygon.o \
		$(HOSTTYPE)/prolog.o $(HOSTTYPE)/vector.o

$(HOSTTYPE)/hpgl2ps:	$(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(DRVLIBS)
	$(STRIP) $@
$(OBJS):	hpgl2ps.h

#
# End of "$Id: Makefile,v 1.2 1999/03/06 18:02:26 mike Exp $".
#
