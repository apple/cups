#
# Documentation makefile for CUPS.
#
# Copyright © 2007-2019 by Apple Inc.
# Copyright © 1997-2007 by Easy Software Products.
#
# Licensed under Apache License v2.0.  See the file "LICENSE" for more
#s information.
#

include ../Makedefs

#
# Document files...
#

WEBPAGES	=	\
			apple-touch-icon.png \
			cups.css \
			cups-printable.css \
			index.html \
			robots.txt
WEBIMAGES	=	\
			images/color-wheel.png \
			images/cups.png \
			images/cups-icon.png \
			images/generic.png \
			images/left.gif \
			images/right.gif \
			images/sel.gif \
			images/unsel.gif \
			images/wait.gif
HELPIMAGES	=	\
			images/cups-block-diagram.png \
			images/cups-command-chain.png \
			images/cups-postscript-chain.png \
			images/cups-raster-chain.png \
			images/raster.png \
			images/raster-organization.png \
			images/sample-image.png \
			images/smiley.jpg
HELPFILES	=	\
			help/accounting.html \
			help/admin.html \
			help/api-admin.html \
			help/api-filter.html \
			help/api-ppd.html \
			help/api-raster.html \
			help/cgi.html \
			help/cupspm.html \
			help/encryption.html \
			help/firewalls.html \
			help/glossary.html \
			help/kerberos.html \
			help/license.html \
			help/man-backend.html \
			help/man-cancel.html \
			help/man-classes.conf.html \
			help/man-client.conf.html \
			help/man-cups.html \
			help/man-cups-config.html \
			help/man-cups-files.conf.html \
			help/man-cups-lpd.html \
			help/man-cups-snmp.html \
			help/man-cupsaccept.html \
			help/man-cupsd.conf.html \
			help/man-cupsd.html \
			help/man-cupsd-helper.html \
			help/man-cupsd-logs.html \
			help/man-cupsenable.html \
			help/man-cupstestppd.html \
			help/man-filter.html \
			help/man-ippevepcl.html \
			help/man-ippeveprinter.html \
			help/man-ipptool.html \
			help/man-ipptoolfile.html \
			help/man-lp.html \
			help/man-lpadmin.html \
			help/man-lpc.html \
			help/man-lpinfo.html \
			help/man-lpmove.html \
			help/man-lpoptions.html \
			help/man-lpq.html \
			help/man-lpr.html \
			help/man-lprm.html \
			help/man-lpstat.html \
			help/man-mime.convs.html \
			help/man-mime.types.html \
			help/man-notifier.html \
			help/man-ppdc.html \
			help/man-ppdhtml.html \
			help/man-ppdi.html \
			help/man-ppdmerge.html \
			help/man-ppdpo.html \
			help/man-printers.conf.html \
			help/man-subscriptions.conf.html \
			help/network.html \
			help/options.html \
			help/overview.html \
			help/policies.html \
			help/postscript-driver.html \
			help/ppd-compiler.html \
			help/raster-driver.html \
			help/ref-ppdcfile.html \
			help/security.html \
			help/sharing.html \
			help/spec-banner.html \
			help/spec-command.html \
			help/spec-design.html \
			help/spec-ipp.html \
			help/spec-ppd.html \
			help/spec-raster.html \
			help/spec-stp.html \
			help/translation.html


#
# Make all documents...
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

install-data: $(INSTALL_LANGUAGES)
	$(INSTALL_DIR) -m 755 $(DOCDIR)
	for file in $(WEBPAGES); do \
		$(INSTALL_MAN) $$file $(DOCDIR); \
	done
	$(INSTALL_DIR) -m 755 $(DOCDIR)/help
	for file in $(HELPFILES); do \
		$(INSTALL_MAN) $$file $(DOCDIR)/help; \
	done
	if test "x$(IPPFIND_MAN)" != x; then \
		$(INSTALL_MAN) help/man-ippfind.html $(DOCDIR)/help; \
	fi
	$(INSTALL_DIR) -m 755 $(DOCDIR)/images
	for file in $(WEBIMAGES) $(HELPIMAGES); do \
		$(INSTALL_MAN) $$file $(DOCDIR)/images; \
	done

install-languages:
	for lang in $(LANGUAGES); do \
		if test -d $$lang; then \
			$(INSTALL_DIR) -m 755 $(DOCDIR)/$$lang; \
			$(INSTALL_DATA) $$lang/index.html $(DOCDIR)/$$lang; \
			$(INSTALL_DATA) $$lang/cups.css $(DOCDIR)/$$lang >/dev/null 2>&1 || true; \
		fi; \
	done

install-langbundle:


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
# Uninstall all documentation files...
#

uninstall: $(UNINSTALL_LANGUAGES)
	for file in $(WEBPAGES); do \
		$(RM) $(DOCDIR)/$$file; \
	done
	for file in $(HELPFILES); do \
		$(RM) $(DOCDIR)/help/$$file; \
	done
	if test "x$(IPPFIND_MAN)" != x; then \
		$(RM) $(DOCDIR)/help/man-ippfind.html; \
	done
	for file in $(WEBIMAGES); do \
		$(RM) $(DOCDIR)/images/$$file; \
	done
	-$(RMDIR) $(DOCDIR)/images
	-$(RMDIR) $(DOCDIR)/help
	-$(RMDIR) $(DOCDIR)

uninstall-languages:
	-for lang in $(LANGUAGES); do \
		$(RM) $(DOCDIR)/$$lang/index.html; \
		$(RM) $(DOCDIR)/$$lang/cups.css; \
		$(RMDIR) $(DOCDIR)/$$lang; \
	done

install-langbundle:
