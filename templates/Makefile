#
# "$Id$"
#
#   Template makefile for the Common UNIX Printing System (CUPS).
#
#   Copyright 1993-2005 by Easy Software Products.
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
#       Hollywood, Maryland 20636 USA
#
#       Voice: (301) 373-9600
#       EMail: cups-info@cups.org
#         WWW: http://www.cups.org
#

include ../Makedefs

#
# Template files...
#

FILES	=	\
		add-class.tmpl \
		add-printer.tmpl \
		admin.tmpl \
		choose-device.tmpl \
		choose-make.tmpl \
		choose-model.tmpl \
		choose-serial.tmpl \
		choose-uri.tmpl \
		class-added.tmpl \
		class-confirm.tmpl \
		class-deleted.tmpl \
		class-jobs-header.tmpl \
		class-modified.tmpl \
		classes.tmpl \
		classes-header.tmpl \
		edit-config.tmpl \
		error.tmpl \
		error-op.tmpl \
		header.tmpl \
		help-header.tmpl \
		job-cancel.tmpl \
		job-hold.tmpl \
		job-move.tmpl \
		job-moved.tmpl \
		job-release.tmpl \
		job-restart.tmpl \
		jobs.tmpl \
		jobs-header.tmpl \
		modify-class.tmpl \
		modify-printer.tmpl \
		option-boolean.tmpl \
		option-conflict.tmpl \
		option-header.tmpl \
		option-pickmany.tmpl \
		option-pickone.tmpl \
		option-trailer.tmpl \
		pager.tmpl \
		printer-accept.tmpl \
		printer-added.tmpl \
		printer-configured.tmpl \
		printer-confirm.tmpl \
		printer-default.tmpl \
		printer-deleted.tmpl \
		printer-jobs-header.tmpl \
		printer-modified.tmpl \
		printer-purge.tmpl \
		printer-reject.tmpl \
		printer-start.tmpl \
		printer-stop.tmpl \
		printers.tmpl \
		printers-header.tmpl \
		restart.tmpl \
		samba-export.tmpl \
		samba-exported.tmpl \
		search.tmpl \
		set-printer-options-header.tmpl \
		set-printer-options-trailer.tmpl \
		test-page.tmpl \
		trailer.tmpl \
		users.tmpl


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

install: all
	$(INSTALL_DIR) $(DATADIR)/templates
	for file in $(FILES); do \
		$(INSTALL_DATA) $$file $(DATADIR)/templates; \
	done


#
# End of "$Id$".
#
