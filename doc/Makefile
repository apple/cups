#
# "$Id$"
#
#   Documentation makefile for the Common UNIX Printing System (CUPS).
#
#   Copyright 2007 by Apple Inc.
#   Copyright 1997-2007 by Easy Software Products.
#
#   These coded instructions, statements, and computer programs are the
#   property of Apple Inc. and are protected by Federal copyright
#   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
#   which should have been included with this file.  If this file is
#   file is missing or damaged, see the license at "http://www.cups.org/".
#

include ../Makedefs

#
# Document files...
#

WEBPAGES	=	\
			cups.css \
			cups-printable.css \
			favicon.ico \
			index.html \
			robots.txt
WEBBUTTONS	=	\
			images/button-accept-jobs.gif \
			images/button-add-class.gif \
			images/button-add-printer.gif \
			images/button-add-rss-subscription.gif \
			images/button-add-this-printer.gif \
			images/button-cancel-all-jobs.gif \
			images/button-cancel-job.gif \
			images/button-cancel-subscription.gif \
			images/button-change-settings.gif \
			images/button-clean-print-heads.gif \
			images/button-clear.gif \
			images/button-continue.gif \
			images/button-delete-class.gif \
			images/button-delete-printer.gif \
			images/button-edit-configuration-file.gif \
			images/button-export-samba.gif \
			images/button-find-new-printers.gif \
			images/button-help.gif \
			images/button-hold-job.gif \
			images/button-manage-classes.gif \
			images/button-manage-jobs.gif \
			images/button-manage-printers.gif \
			images/button-manage-server.gif \
			images/button-modify-class.gif \
			images/button-modify-printer.gif \
			images/button-move-job.gif \
			images/button-move-jobs.gif \
			images/button-print-self-test-page.gif \
			images/button-print-test-page.gif \
			images/button-publish-printer.gif \
			images/button-reject-jobs.gif \
			images/button-release-job.gif \
			images/button-restart-job.gif \
			images/button-save-changes.gif \
			images/button-search.gif \
			images/button-set-allowed-users.gif \
			images/button-set-as-default.gif \
			images/button-set-printer-options.gif \
			images/button-show-active.gif \
			images/button-show-all.gif \
			images/button-show-completed.gif \
			images/button-show-next.gif \
			images/button-show-previous.gif \
			images/button-sort-ascending.gif \
			images/button-sort-descending.gif \
			images/button-start-class.gif \
			images/button-start-printer.gif \
			images/button-stop-class.gif \
			images/button-stop-printer.gif \
			images/button-unpublish-printer.gif \
			images/button-use-default-config.gif \
			images/button-view-access-log.gif \
			images/button-view-error-log.gif \
			images/button-view-page-log.gif \
			images/button-view-printable-version.gif
WEBIMAGES	=	\
			images/bottom-left.gif \
			images/bottom-right.gif \
			images/classes.gif \
			images/cups-block-diagram.gif \
			images/esp-logo.gif \
			images/happy.gif \
			images/logo.gif \
			images/printer-idle.gif \
			images/printer-processing.gif \
			images/printer-stopped.gif \
			images/raster.png \
			images/raster.svg \
			images/smiley.jpg \
			images/tab-left.gif \
			images/tab-right.gif \
			images/top-left.gif \
			images/top-middle.gif \
			images/top-right.gif \
			images/web-add-this-printer.gif \
			images/web-password.gif \
			images/web-printer-driver.gif \
			images/web-printer-status.gif \
			images/web-set-printer-options.gif \
			images/web-test-page.gif
HELPFILES	=	\
			help/accounting.html \
			help/api-array.html \
			help/api-cups.html \
			help/api-filedir.html \
			help/api-filter.html \
			help/api-httpipp.html \
			help/api-ppd.html \
			help/api-raster.html \
			help/cgi.html \
			help/glossary.html \
			help/kerberos.html \
			help/license.html \
			help/man-accept.html \
			help/man-backend.html \
			help/man-cancel.html \
			help/man-classes.conf.html \
			help/man-client.conf.html \
			help/man-cups-config.html \
			help/man-cups-lpd.html \
			help/man-cups-polld.html \
			help/man-cupsaddsmb.html \
			help/man-cupsd.conf.html \
			help/man-cupsd.html \
			help/man-cupsenable.html \
			help/man-cupstestdsc.html \
			help/man-cupstestppd.html \
			help/man-filter.html \
			help/man-lp.html \
			help/man-lpadmin.html \
			help/man-lpc.html \
			help/man-lpinfo.html \
			help/man-lpmove.html \
			help/man-lpoptions.html \
			help/man-lppasswd.html \
			help/man-lpq.html \
			help/man-lpr.html \
			help/man-lprm.html \
			help/man-lpstat.html \
			help/man-mime.convs.html \
			help/man-mime.types.html \
			help/man-printers.conf.html \
			help/network.html \
			help/options.html \
			help/overview.html \
			help/policies.html \
			help/ref-access_log.html \
			help/ref-classes-conf.html \
			help/ref-client-conf.html \
			help/ref-cupsd-conf.html \
			help/ref-error_log.html \
			help/ref-mailto-conf.html \
			help/ref-page_log.html \
			help/ref-printers-conf.html \
			help/ref-snmp-conf.html \
			help/ref-subscriptions-conf.html \
			help/security.html \
			help/spec-cmp.html \
			help/spec-command.html \
			help/spec-design.html \
			help/spec-ipp.html \
			help/spec-postscript.html \
			help/spec-ppd.html \
			help/spec-raster.html \
			help/spec-stp.html \
			help/standard.html \
			help/translation.html \
			help/whatsnew.html


#
# Make all documents...
#

all:


#
# Remove all generated files...
#

clean:


#
# Dummy depend target...
#

depend:


#
# Install all documentation files...
#

install:	all $(INSTALL_LANGUAGES)
	$(INSTALL_DIR) -m 755 $(DOCDIR)
	for file in $(WEBPAGES); do \
		$(INSTALL_MAN) $$file $(DOCDIR); \
	done
	$(INSTALL_DIR) -m 755 $(DOCDIR)/help
	for file in $(HELPFILES); do \
		$(INSTALL_MAN) $$file $(DOCDIR)/help; \
	done
	$(INSTALL_DIR) -m 755 $(DOCDIR)/images
	for file in $(WEBIMAGES) $(WEBBUTTONS); do \
		$(INSTALL_MAN) $$file $(DOCDIR)/images; \
	done

install-languages:
	for lang in $(LANGUAGES); do \
		$(INSTALL_DIR) -m 755 $(DOCDIR)/$$lang/images; \
		if test -f $$lang/index.html; then \
			$(INSTALL_MAN) $$lang/index.html $(DOCDIR)/$$lang; \
		fi; \
		if test -f $$lang/cups.css; then \
			$(INSTALL_MAN) $$lang/cups.css $(DOCDIR)/$$lang; \
		fi; \
		for file in $(WEBBUTTONS); do \
			if test -f $$lang/$$file; then \
				$(INSTALL_MAN) $$lang/$$file $(DOCDIR)/$$lang/images; \
			fi; \
		done \
	done


#
# Unnstall all documentation files...
#

uninstall: $(UNINSTALL_LANGUAGES)
	for file in $(WEBPAGES); do \
		$(RM) $(DOCDIR)/$$file; \
	done
	for file in $(HELPFILES); do \
		$(INSTALL_MAN) $(DOCDIR)/help/$$file; \
	done
	for file in $(WEBIMAGES) $(WEBBUTTONS); do \
		$(INSTALL_MAN) $(DOCDIR)/images/$$file; \
	done
	-$(RMDIR) $(DOCDIR)/images
	-$(RMDIR) $(DOCDIR)/help
	-$(RMDIR) $(DOCDIR)

uninstall-languages:
	-for lang in $(LANGUAGES); do \
		$(RM) $(DOCDIR)/$$lang/index.html; \
		$(RM) $(DOCDIR)/$$lang/cups.css; \
		for file in $(WEBBUTTONS); do \
			$(RM) $(DOCDIR)/$$lang/images/$$file; \
		done \
		$(RMDIR) $(DOCDIR)/$$lang/images; \
		$(RMDIR) $(DOCDIR)/$$lang; \
	done


#
# End of Makefile.
#
