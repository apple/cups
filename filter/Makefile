#!/usr/sbin/smake
#
# "$Id: Makefile,v 1.1 1996/08/24 19:41:24 mike Exp $"
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
#   Revision 1.1  1996/08/24 19:41:24  mike
#   Initial revision
#
#

#
# Standard definitions...
#

include ../Makedefs


TARGETS	=	hpgl2ps

#
# Make all targets...
#

all:	$(TARGETS)

#
# Clean all garbage...
#

clean:

#
# hpgl2ps
#

OBJS	=	attr.$(OBJ)	char.$(OBJ)	config.$(OBJ) \
		input.$(OBJ)	main.$(OBJ)	polygon.$(OBJ) \
		prolog.$(OBJ)	vector.$(OBJ)

hpgl2ps:	$(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LIBS)
$(OBJS):	hpgl2ps.h

#
# End of "$Id: Makefile,v 1.1 1996/08/24 19:41:24 mike Exp $".
#
