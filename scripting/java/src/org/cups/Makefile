#
# "$Id: Makefile,v 1.1 2003/04/11 18:23:41 mike Exp $"
#
#   Java Makefile for the Common UNIX Printing System (CUPS).
#
#   Copyright 1997-2003 by Easy Software Products, all rights reserved.
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

#
# Default build rules and suffixes...
#

.SUFFIXES:	.class .jar .java
.SILENT:

.java.class:
	echo Compiling $@...
	javac -d ../.. -classpath ../.. $<


#
# List of classes...
#

CLASSES	=	\
		IPPBase64Encoder.class \
		IPPDefs.class \
		IPPError.class \
		IPPStatus.class \
		IPPCalendar.class \
		IPPValue.class \
		IPPAttribute.class \
		IPPRequest.class \
		IPPURLConnection.class \
		IPP.class \
		IPPHttp.class \
		CupsJob.class \
		Cups.class \
		CupsPrinter.class


#
# Make everything...
#

all:	../../../cups.jar


#
# Clean everything...
#

clean:
	rm -f $(CLASSES)


#
# Make documentation using javadoc...
#

docs:
	javadoc -d ../../../docs -classpath ../.. -verbose $(CLASSES:.class=.java)


#
# Make CUPS class archive...
#

../../../cups.jar:	$(CLASSES)
	rm -f ../../../cups.jar
	jar cvf ../../../cups.jar $(CLASSES)


#
# End of "$Id: Makefile,v 1.1 2003/04/11 18:23:41 mike Exp $".
#
