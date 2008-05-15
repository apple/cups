#
# "$Id$"
#
#   Documentation makefile for the Common UNIX Printing System (CUPS).
#
#   Copyright 2007-2008 by Apple Inc.
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
			favicon.png \
			index.html \
			robots.txt
WEBIMAGES	=	\
			images/classes.png \
			images/cups.png \
			images/cups-block-diagram.gif \
			images/cups-icon.png \
			images/left.gif \
			images/printer-idle.png \
			images/printer-processing.png \
			images/printer-stopped.png \
			images/raster.png \
			images/right.gif \
			images/sel.gif \
			images/smiley.jpg \
			images/unsel.gif \
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
			help/api-overview.html \
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
# Make library targets...
#

libs:


#
# Remove all generated files...
#

clean:


#
# Dummy depend target...
#

depend:


#
# Install all targets...
#

install:	all install-data install-headers install-libs install-exec


#
# Install data files...
#

install-data:
	$(INSTALL_DIR) -m 755 $(DOCDIR)
	for file in $(WEBPAGES); do \
		$(INSTALL_MAN) $$file $(DOCDIR); \
	done
	$(INSTALL_DIR) -m 755 $(DOCDIR)/help
	for file in $(HELPFILES); do \
		$(INSTALL_MAN) $$file $(DOCDIR)/help; \
	done
	$(INSTALL_DIR) -m 755 $(DOCDIR)/images
	for file in $(WEBIMAGES); do \
		$(INSTALL_MAN) $$file $(DOCDIR)/images; \
	done

install-languages:
	for lang in $(LANGUAGES); do \
		if test -d $$lang; then \
			$(INSTALL_DIR) -m 755 $(DOCDIR)/$$lang/images; \
			$(INSTALL_MAN) $$lang/index.html $(DOCDIR)/$$lang; \
			$(INSTALL_MAN) $$lang/cups.css $(DOCDIR)/$$lang 2>/dev/null || true; \
			$(INSTALL_DIR) -m 755 $(DOCDIR)/$$lang/images; \
			for file in $(WEBIMAGES); do \
				$(INSTALL_MAN) $$lang/$$file $(DOCDIR)/$$lang/images 2>/dev/null || true; \
			done; \
		fi; \
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
# Unnstall all documentation files...
#

uninstall: $(UNINSTALL_LANGUAGES)
	for file in $(WEBPAGES); do \
		$(RM) $(DOCDIR)/$$file; \
	done
	for file in $(HELPFILES); do \
		$(RM) $(DOCDIR)/$$file; \
	done
	for file in $(WEBIMAGES); do \
		$(RM) $(DOCDIR)/$$file; \
	done
	-$(RMDIR) $(DOCDIR)/images
	-$(RMDIR) $(DOCDIR)/help
	-$(RMDIR) $(DOCDIR)

uninstall-languages:
	-for lang in $(LANGUAGES); do \
		$(RM) $(DOCDIR)/$$lang/index.html; \
		$(RM) $(DOCDIR)/$$lang/cups.css; \
		for file in $(WEBIMAGES); do \
			$(RM) $(DOCDIR)/$$lang/$$file; \
		done; \
		$(RMDIR) $(DOCDIR)/$$lang/images; \
		$(RMDIR) $(DOCDIR)/$$lang; \
	done


#
# End of Makefile.
#
