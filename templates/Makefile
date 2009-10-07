#
# "$Id: Makefile 7940 2008-09-16 00:45:16Z mike $"
#
#   Template makefile for the Common UNIX Printing System (CUPS).
#
#   Copyright 2007-2009 by Apple Inc.
#   Copyright 1993-2007 by Easy Software Products.
#
#   These coded instructions, statements, and computer programs are the
#   property of Apple Inc. and are protected by Federal copyright
#   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
#   which should have been included with this file.  If this file is
#   file is missing or damaged, see the license at "http://www.cups.org/".
#

include ../Makedefs

#
# Template files...
#

FILES	=	\
		add-class.tmpl \
		add-printer.tmpl \
		add-rss-subscription.tmpl \
		admin.tmpl \
		choose-device.tmpl \
		choose-make.tmpl \
		choose-model.tmpl \
		choose-serial.tmpl \
		choose-uri.tmpl \
		class.tmpl \
		class-added.tmpl \
		class-confirm.tmpl \
		class-deleted.tmpl \
		class-jobs-header.tmpl \
		class-modified.tmpl \
		classes.tmpl \
		classes-header.tmpl \
		command.tmpl \
		edit-config.tmpl \
		error.tmpl \
		error-op.tmpl \
		header.tmpl \
		help-header.tmpl \
		help-trailer.tmpl \
		help-printable.tmpl \
		job-cancel.tmpl \
		job-hold.tmpl \
		job-move.tmpl \
		job-moved.tmpl \
		job-release.tmpl \
		job-restart.tmpl \
		jobs.tmpl \
		jobs-header.tmpl \
		list-available-printers.tmpl \
		modify-class.tmpl \
		modify-printer.tmpl \
		norestart.tmpl \
		option-boolean.tmpl \
		option-conflict.tmpl \
		option-header.tmpl \
		option-pickmany.tmpl \
		option-pickone.tmpl \
		option-trailer.tmpl \
		pager.tmpl \
		printer.tmpl \
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
		subscription-added.tmpl \
		subscription-canceled.tmpl \
		test-page.tmpl \
		trailer.tmpl \
		users.tmpl


#
# Make everything...
#

all:


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


#
# Dummy depend...
#

depend:


#
# Install all targets...
#

install:	all install-data install-headers install-libs install-exec


#
# Install data files...
#

install-data: $(INSTALL_LANGUAGES)
	$(INSTALL_DIR) -m 755 $(DATADIR)/templates
	for file in $(FILES); do \
		$(INSTALL_DATA) $$file $(DATADIR)/templates; \
	done

install-languages:
	for lang in $(LANGUAGES); do \
		if test -d $$lang; then \
			$(INSTALL_DIR) -m 755 $(DATADIR)/templates/$$lang; \
			for file in $(FILES); do \
				$(INSTALL_DATA) $$lang/$$file $(DATADIR)/templates/$$lang >/dev/null 2>&1 || true; \
			done \
		fi \
	done



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

uninstall: $(UNINSTALL_LANGUAGES)
	for file in $(FILES); do \
		$(RM) $(DATADIR)/templates/$$file; \
	done
	-$(RMDIR) $(DATADIR)/templates

uninstall-languages:
	for lang in $(LANGUAGES); do \
		for file in $(FILES); do \
			$(RM) $(DATADIR)/templates/$$lang/$$file; \
		done \
		$(RMDIR) $(DATADIR)/templates/$$lang; \
	done


#
# End of "$Id: Makefile 7940 2008-09-16 00:45:16Z mike $".
#
