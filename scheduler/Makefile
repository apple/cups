#
# "$Id: Makefile,v 1.2 1999/01/24 14:25:11 mike Exp $"
#
#   Scheduler Makefile for the Common UNIX Printing System (CUPS).
#
#   Copyright 1997-1999 by Easy Software Products, all rights reserved.
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
#       44145 Airport View Drive, Suite 204
#       Hollywood, Maryland 20636-3111 USA
#
#       Voice: (301) 373-9603
#       EMail: cups-info@cups.org
#         WWW: http://www.cups.org
#

include ../Makedefs

OBJS	=	auth.o classes.o conf.o dirsvc.o http.o main.o job.o printers.o

cupsd:	$(OBJS)
	$(CC) $(LDFLAGS) -o cupsd $(OBJS) -L../libmime -lmime $(SOCKETLIBS)

$(OBJS):	auth.h classes.h conf.h cupsd.h dirsvc.h http.h job.h \
		printers.h ../libmime/mime.h ../libcups/cups.h ../Makedefs

#
# End of "$Id: Makefile,v 1.2 1999/01/24 14:25:11 mike Exp $".
#
