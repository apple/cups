### ----------------- CUPS Ghostscript Driver ---------------------- ###

cups_=	$(GLOBJ)gdevcups.$(OBJ)

$(DD)cups.dev:	$(cups_) $(GLD)page.dev
	$(ADDMOD) $(DD)cups -lib cupsimage -lib cups
	$(SETPDEV2) $(DD)cups $(cups_)

$(GLOBJ)gdevcups.$(OBJ): $(GLSRC)gdevcups.c $(PDEVH)
	$(GLCC) $(GLOBJ)gdevcups.$(OBJ) $(C_) $(GLSRC)gdevcups.c

install:	install-cups

install-cups:
	-mkdir -p `cups-config --serverbin`/filter
	$(INSTALL_PROGRAM) pstoraster `cups-config --serverbin`/filter
	-mkdir -p `cups-config --serverroot`
	$(INSTALL_PROGRAM) pstoraster.convs `cups-config --serverroot`
