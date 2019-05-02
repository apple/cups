#
# Example files makefile for CUPS.
#
# Copyright © 2007-2019 by Apple Inc.
# Copyright © 2002-2005 by Easy Software Products.
#
# Licensed under Apache License v2.0.  See the file "LICENSE" for more
# information.
#

#
# Include standard definitions...
#

include ../Makedefs


#
# Examples...
#

DRVFILES 	=	\
			color.drv \
			constraint.drv \
			custom.drv \
			grouping.drv \
			laserjet-basic.drv \
			laserjet-pjl.drv \
			minimum.drv \
			postscript.drv \
			r300-basic.drv \
			r300-colorman.drv \
			r300-remote.drv
DATAFILES	=	\
			color.jpg \
			document-a4.pdf \
			document-a4.ps \
			document-letter.pdf \
			document-letter.ps \
			gray.jpg \
			onepage-a4.pdf \
			onepage-a4.ps \
			onepage-letter.pdf \
			onepage-letter.ps \
			testfile.jpg \
			testfile.pcl \
			testfile.pdf \
			testfile.ps \
			testfile.txt
TESTFILES	=	\
			cancel-current-job.test \
			create-job-format.test \
			create-job-sheets.test \
			create-job-timeout.test \
			create-job.test \
			create-printer-subscription.test \
			cups-create-local-printer.test \
			fax-job.test \
			get-completed-jobs.test \
			get-devices.test \
			get-job-attributes.test \
			get-job-attributes2.test \
			get-job-template-attributes.test \
			get-jobs.test \
			get-notifications.test \
			get-ppd-printer.test \
			get-ppd.test \
			get-ppds-drv-only.test \
			get-ppds-language.test \
			get-ppds-make-and-model.test \
			get-ppds-make.test \
			get-ppds-product.test \
			get-ppds-psversion.test \
			get-ppds.test \
			get-printer-attributes-suite.test \
			get-printer-attributes.test \
			get-printer-description-attributes.test \
			get-printers-printer-id.test \
			get-printers.test \
			get-subscriptions.test \
			identify-printer-display.test \
			identify-printer-multiple.test \
			identify-printer.test \
			ipp-1.1.test \
			ipp-2.0.test \
			ipp-2.1.test \
			ipp-2.2.test \
			ipp-backend.test \
			ipp-everywhere.test \
			print-job-and-wait.test \
			print-job-deflate.test \
			print-job-gzip.test \
			print-job-hold.test \
			print-job-letter.test \
			print-job-manual.test \
			print-job-media-col.test \
			print-job-password.test \
			print-job.test \
			print-uri.test \
			set-attrs-hold.test \
			validate-job.test


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
# Clean everything...
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

install-data:
	echo Installing sample PPD compiler files in $(DATADIR)/examples...
	$(INSTALL_DIR) $(DATADIR)/examples
	for file in $(DRVFILES); do \
		$(INSTALL_DATA) $$file $(DATADIR)/examples; \
	done
	echo Installing sample ipptool files in $(DATADIR)/ipptool...
	$(INSTALL_DIR) -m 755 $(DATADIR)/ipptool
	for file in $(DATAFILES); do \
		$(INSTALL_COMPDATA) $$file $(DATADIR)/ipptool; \
	done
	for file in $(TESTFILES); do \
		$(INSTALL_DATA) $$file $(DATADIR)/ipptool; \
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

uninstall:
	echo Uninstalling sample PPD compiler files from $(DATADIR)/examples...
	for file in $(DRVFILES); do \
		$(RM) $(DATADIR)/examples/$$file; \
	done
	-$(RMDIR) $(DATADIR)/examples
	echo Uninstalling sample ipptool files from $(DATADIR)/ipptool...
	for file in $(DATAFILES); do \
		$(RM) $(DATADIR)/ipptool/$$file; \
	done
	for file in $(TESTFILES); do \
		$(RM) $(DATADIR)/ipptool/$$file; \
	done
	-$(RMDIR) $(DATADIR)/ipptool
