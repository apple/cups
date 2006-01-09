#
# "$Id$"
#
#   Documentation makefile for the Common UNIX Printing System (CUPS).
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
# Document files...
#

WEBPAGES	=	cups.css cupsdoc.css index.html robots.txt
WEBIMAGES	=	\
			images/accept-jobs.gif \
			images/add-class.gif \
			images/add-printer.gif \
			images/add-this-printer.gif \
			images/bottom-left.gif \
			images/bottom-right.gif \
			images/cancel.gif \
			images/cancel-all-jobs.gif \
			images/cancel-job.gif \
			images/change-settings.gif \
			images/classes.gif \
			images/continue.gif \
			images/delete-class.gif \
			images/delete-printer.gif \
			images/draft.gif \
			images/edit-configuration-file.gif \
			images/esp-logo.gif \
			images/happy.gif \
			images/help.gif \
			images/hold-job.gif \
			images/logo.gif \
			images/manage-classes.gif \
			images/manage-jobs.gif \
			images/manage-printers.gif \
			images/manage-server.gif \
			images/modify-class.gif \
			images/modify-printer.gif \
			images/printer-idle.gif \
			images/printer-processing.gif \
			images/printer-stopped.gif \
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
			images/show-completed.gif \
			images/start-class.gif \
			images/start-printer.gif \
			images/stop-class.gif \
			images/stop-printer.gif \
			images/tab-left.gif \
			images/tab-right.gif \
			images/top-left.gif \
			images/top-middle.gif \
			images/top-right.gif \
			images/unpublish-printer.gif \
			images/use-default-config.gif \
			images/view-access-log.gif \
			images/view-error-log.gif \
			images/view-page-log.gif
HELPFILES	=	\
			help/access_log-reference.html \
			help/api-array.html \
			help/api-filedir.html \
			help/api-filter.html \
			help/api-httpipp.html \
			help/api-ppd.html \
			help/classes-conf-reference.html \
			help/client-conf-reference.html \
			help/cupsd-conf-reference.html \
			help/error_log-reference.html \
			help/page_log-reference.html \
			help/printers-conf-reference.html \
			help/subscriptions-conf-reference.html \
			help/man-accept.html \
			help/man-backend.html \
			help/man-cupsaddsmb.html \
			help/man-cups-config.html \
			help/man-cupsd.html \
			help/man-cupsenable.html \
			help/man-cups-lpd.html \
			help/man-cups-polld.html \
			help/man-cupstestppd.html \
			help/man-filter.html \
			help/man-lpadmin.html \
			help/man-lpc.html \
			help/man-lp.html \
			help/man-lpinfo.html \
			help/man-lpmove.html \
			help/man-lpoptions.html \
			help/man-lppasswd.html \
			help/man-lpq.html \
			help/man-lpr.html \
			help/man-lprm.html \
			help/man-lpstat.html \
			help/network.html \
			help/overview.html \
			help/spec-ppd.html \
			help/standard.html \
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

install:
	$(INSTALL_DIR) $(DOCDIR)
	for file in $(WEBPAGES); do \
		$(INSTALL_MAN) $$file $(DOCDIR); \
	done
	$(INSTALL_DIR) $(DOCDIR)/help
	for file in $(HELPFILES); do \
		$(INSTALL_MAN) $$file $(DOCDIR)/help; \
	done
	$(INSTALL_DIR) $(DOCDIR)/images
	for file in $(WEBIMAGES); do \
		$(INSTALL_MAN) $$file $(DOCDIR)/images; \
	done


#
# End of Makefile.
#
