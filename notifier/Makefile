#
# "$Id: Makefile,v 1.1.2.1 2001/04/02 19:53:14 mike Exp $"
#
#   Notifier makefile for the Common UNIX Printing System (CUPS).
#
#   Copyright 1997-2001 by Easy Software Products, all rights reserved.
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

TARGETS =	mailto
OBJS	=	mailto.o


#
# Make all targets...
#

all:	$(TARGETS)


#
# Clean all object files...
#

clean:
	$(RM) $(OBJS) $(TARGETS)


#
# Install all targets...
#

install:
	-$(MKDIR) $(SERVERBIN)/notifier
	$(CHMOD) ugo+rx $(SERVERBIN)
	$(CHMOD) ugo+rx $(SERVERBIN)/notifier
	$(INSTALL_BIN) $(TARGETS) $(SERVERBIN)/notifier


#
# mailto
#

mailto:	mailto.o ../cups/$(LIBCUPS)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o mailto mailto.o $(LIBS)

mailto.o:	../cups/cups.h ../cups/ipp.h ../cups/language.h	\
		../cups/string.h


$(OBJS):	../config.h ../Makedefs

#
# End of "$Id: Makefile,v 1.1.2.1 2001/04/02 19:53:14 mike Exp $".
#
