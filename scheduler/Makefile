#
# "$Id: Makefile,v 1.5 1999/03/01 22:26:15 mike Exp $"
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

OBJS	=	auth.o classes.o client.o conf.o dirsvc.o main.o ipp.o job.o \
		printers.o

#
# Make the scheduler executable, "cupsd".
#

cupsd:	$(OBJS) ../cups/libcups.a
	$(CC) $(LDFLAGS) -o cupsd $(OBJS) $(LIBS)

$(OBJS):	auth.h classes.h client.h conf.h cupsd.h dirsvc.h job.h \
		printers.h ../cups/cups.h ../cups/http.h ../cups/ipp.h \
		../cups/mime.h ../cups/string.h ../Makedefs

#
# Clean all object files...
#

clean:
	rm -f $(OBJS) cupsd

#
# Install the scheduler...
#

install:

#
# End of "$Id: Makefile,v 1.5 1999/03/01 22:26:15 mike Exp $".
#
