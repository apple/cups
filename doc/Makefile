#
# "$Id$"
#
#   Documentation makefile for the Common UNIX Printing System (CUPS).
#
#   Copyright 1997-2006 by Easy Software Products.
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
# Document files...
#

LANGUAGES	=	ja
WEBPAGES	=	\
			cups.css \
			cupsdoc.css \
			favicon.ico \
			index.html \
			robots.txt
WEBBUTTONS	=	\
			images/accept-jobs.gif \
			images/add-class.gif \
			images/add-printer.gif \
			images/add-this-printer.gif \
			images/cancel.gif \
			images/cancel-all-jobs.gif \
			images/cancel-job.gif \
			images/change-settings.gif \
			images/clean-print-heads.gif \
			images/clear.gif \
			images/continue.gif \
			images/delete-class.gif \
			images/delete-printer.gif \
			images/edit-configuration-file.gif \
			images/export-samba.gif \
			images/help.gif \
			images/hold-job.gif \
			images/manage-classes.gif \
			images/manage-jobs.gif \
			images/manage-printers.gif \
			images/manage-server.gif \
			images/modify-class.gif \
			images/modify-printer.gif \
			images/move-job.gif \
			images/move-jobs.gif \
			images/print-self-test-page.gif \
			images/print-test-page.gif \
			images/publish-printer.gif \
			images/reject-jobs.gif \
			images/release-job.gif \
			images/restart-job.gif \
			images/save-changes.gif \
			images/search.gif \
			images/set-allowed-users.gif \
			images/set-as-default.gif \
			images/set-printer-options.gif \
			images/show-active.gif \
			images/show-all.gif \
			images/show-completed.gif \
			images/show-next.gif \
			images/show-previous.gif \
			images/sort-ascending.gif \
			images/sort-descending.gif \
			images/start-class.gif \
			images/start-printer.gif \
			images/stop-class.gif \
			images/stop-printer.gif \
			images/unpublish-printer.gif \
			images/use-default-config.gif \
			images/view-access-log.gif \
			images/view-error-log.gif \
			images/view-page-log.gif
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
			help/api-array.html \
			help/api-cups.html \
			help/api-filedir.html \
			help/api-filter.html \
			help/api-httpipp.html \
			help/api-ppd.html \
			help/api-raster.html \
			help/glossary.html \
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
			help/ref-access_log.html \
			help/ref-classes-conf.html \
			help/ref-client-conf.html \
			help/ref-cupsd-conf.html \
			help/ref-error_log.html \
			help/ref-page_log.html \
			help/ref-printers-conf.html \
			help/ref-subscriptions-conf.html \
			help/security.html \
			help/spec-cmp.html \
			help/spec-command.html \
			help/spec-design.html \
			help/spec-ipp.html \
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
# Install all documentation files...
#

install:	all
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
	for lang in $(LANGUAGES); do \
		$(INSTALL_DIR) -m 755 $(DOCDIR)/$$lang/images; \
		$(INSTALL_MAN) $$lang/index.html $(DOCDIR)/$$lang; \
		for file in $(WEBBUTTONS); do \
			$(INSTALL_MAN) $$lang/$$file $(DOCDIR)/$$lang/images; \
		done \
	done


#
# End of Makefile.
#
