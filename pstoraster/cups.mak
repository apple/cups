#
# "$Id$"
#
# CUPS driver makefile for Ghostscript.
#
# Copyright 2001-2005 by Easy Software Products.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or (at your option)
# any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
#

### ----------------- CUPS Ghostscript Driver ---------------------- ###

cups_=	$(GLOBJ)gdevcups.$(OBJ)

CUPSSERVER= $(install_prefix)`cups-config --serverbin`
CUPSCONFIG= $(install_prefix)`cups-config --serverroot`
CUPSDATA= $(install_prefix)`cups-config --datadir`

$(DD)cups.dev:	$(cups_) $(GLD)page.dev
	$(ADDMOD) $(DD)cups -lib cupsimage -lib cups
	$(SETPDEV2) $(DD)cups $(cups_)

$(GLOBJ)gdevcups.$(OBJ): pstoraster/gdevcups.c $(PDEVH)
	$(GLCC) $(GLO_)gdevcups.$(OBJ) $(C_) pstoraster/gdevcups.c

install:	install-cups

install-cups:
	-mkdir -p $(CUPSSERVER)/filter
	$(INSTALL_PROGRAM) pstoraster/pstoraster $(CUPSSERVER)/filter
	$(INSTALL_PROGRAM) pstoraster/pstopxl $(CUPSSERVER)/filter
	-mkdir -p $(CUPSCONFIG)
	$(INSTALL_DATA) pstoraster/pstoraster.convs $(CUPSCONFIG)
	-mkdir -p $(CUPSDATA)/model
	$(INSTALL_DATA) pstoraster/pxlcolor.ppd $(CUPSDATA)/model
	$(INSTALL_DATA) pstoraster/pxlmono.ppd $(CUPSDATA)/model


#
# End of "$Id$".
#
