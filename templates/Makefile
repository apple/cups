#
# "$Id: Makefile,v 1.6.2.4 2002/08/16 20:23:48 mike Exp $"
#
#   Template makefile for the Common UNIX Printing System (CUPS).
#
#   Copyright 1993-2002 by Easy Software Products.
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

#
# Template files...
#

FILES	=	add-class.tmpl \
		add-printer.tmpl \
		admin-op.tmpl \
		admin.tmpl \
		choose-device.tmpl \
		choose-make.tmpl \
		choose-members.tmpl \
		choose-model.tmpl \
		choose-serial.tmpl \
		choose-uri.tmpl \
		class-added.tmpl \
		class-confirm.tmpl \
		class-deleted.tmpl \
		class-modified.tmpl \
		classes.tmpl \
		config-printer.tmpl \
		config-printer2.tmpl \
		error.tmpl \
		header.tmpl \
		jobs.tmpl \
		job-cancel.tmpl \
		job-hold.tmpl \
		job-op.tmpl \
		job-release.tmpl \
		job-restart.tmpl \
		modify-class.tmpl \
		modify-printer.tmpl \
		option-boolean.tmpl \
		option-conflict.tmpl \
		option-header.tmpl \
		option-pickmany.tmpl \
		option-pickone.tmpl \
		option-trailer.tmpl \
		printers.tmpl \
		printer-accept.tmpl \
		printer-added.tmpl \
		printer-configured.tmpl \
		printer-confirm.tmpl \
		printer-deleted.tmpl \
		printer-modified.tmpl \
		printer-purge.tmpl \
		printer-reject.tmpl \
		printer-start.tmpl \
		printer-stop.tmpl \
		test-page.tmpl \
		trailer.tmpl


#
# Make everything...
#

all:


#
# Clean all config and object files...
#

clean:


#
# Install files...
#

install:
	$(INSTALL_DIR) $(DATADIR)/templates
	for file in $(FILES); do \
		$(INSTALL_DATA) $$file $(DATADIR)/templates; \
	done


#
# End of "$Id: Makefile,v 1.6.2.4 2002/08/16 20:23:48 mike Exp $".
#
