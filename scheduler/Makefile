#
# "$Id: Makefile,v 1.1 1998/10/16 18:28:01 mike Exp $"
#
#   Interface library Makefile for the Common UNIX Printing System (CUPS).
#
#   Copyright 1997-1998 by Easy Software Products.
#
#       Easy Software Products
#       44145 Airport View Drive, Suite 204
#       Hollywood, Maryland 20636-3111 USA
#
#       Voice: (301) 373-9603
#       EMail: cups-info@cups.org
#         WWW: http://www.cups.org
#
#   PostScript is a trademark of Adobe Systems, Inc.
#
#   This library is free software; you can redistribute it and/or modify it
#   under the terms of the GNU Library General Public License as published
#   by the Free Software Foundation; either version 2 of the License, or
#   (at your option) any later version.
#
#   This library is distributed in the hope that it will be useful, but
#   WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#   GNU General Public License for more details.
#
#   You should have received a copy of the GNU Library General Public
#   License along with this program; if not, write to the Free Software
#   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307,
#   USA.
#

include ../Makedefs

OBJS	=	auth.o classes.o conf.o dirsvc.o http.o main.o printers.o

cupsd:	$(OBJS)
	$(CC) $(LDFLAGS) -o cupsd $(OBJS) -L../libmime -lmime $(SOCKETLIBS)

$(OBJS):	auth.h classes.h conf.h cupsd.h dirsvc.h http.h printers.h \
		../libmime/mime.h ../libcups/cups.h ../Makedefs

#
# End of "$Id: Makefile,v 1.1 1998/10/16 18:28:01 mike Exp $".
#
