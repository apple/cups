#
# "$Id: cups.mak,v 1.1.2.2 2002/04/21 16:11:28 mike Exp $"
#
# CUPS driver makefile for Ghostscript.
#
# Copyright 2001-2002 by Easy Software Products.
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

$(DD)cups.dev:	$(cups_) $(GLD)page.dev
	$(ADDMOD) $(DD)cups -lib cupsimage -lib cups
	$(SETPDEV2) $(DD)cups $(cups_)

$(GLOBJ)gdevcups.$(OBJ): pstoraster/gdevcups.c $(PDEVH)
	$(GLCC) $(GLO_)gdevcups.$(OBJ) $(C_) pstoraster/gdevcups.c

install:	install-cups

install-cups:
	-mkdir -p `cups-config --serverbin`/filter
	$(INSTALL_PROGRAM) pstoraster/pstoraster `cups-config --serverbin`/filter
	-mkdir -p `cups-config --serverroot`
	$(INSTALL_DATA) pstoraster/pstoraster.convs `cups-config --serverroot`


#
# End of "$Id: cups.mak,v 1.1.2.2 2002/04/21 16:11:28 mike Exp $".
#
