#
# "$Id: Makefile,v 1.6.2.4 2002/06/04 20:32:55 mike Exp $"
#
#   pdftops filter Makefile for the Common UNIX Printing System (CUPS).
#
#   CUPS filter changes Copyright 1997-2002 by Easy Software Products.
#   Xpdf code Copyright 1996-1999 by Derek B. Noonburg
#


include ../Makedefs

#
# Object files...
#

LIBOBJS	=	Decrypt.o GString.o gfile.o gmempp.o gmem.o parseargs.o \
		Array.o Catalog.o Dict.o Error.o FontEncoding.o \
		FontFile.o FormWidget.o Function.o Gfx.o GfxFont.o GfxState.o \
		Lexer.o Link.o Object.o OutputDev.o Page.o Params.o \
		Parser.o PDFDoc.o PSOutputDev.o Stream.o XRef.o
OBJS	=	pdftops.o $(LIBOBJS)

#
# Make everything...
#

all:	pdftops


#
# Clean all object files...
#

clean:
	$(RM) $(OBJS)
	$(RM) libxpdf.a
	$(RM) pdftops


#
# Update dependencies (without system header dependencies...)
#

depend:
	makedepend -Y -I.. -fDependencies $(OBJS:.o=.cxx) gmem.c parseargs.c >/dev/null 2>&1


#
# Install the filter...
#

install:	all
	$(INSTALL_DIR) $(SERVERBIN)/filter
	$(INSTALL_BIN) pdftops $(SERVERBIN)/filter


#
# pdftops
#

pdftops:	libxpdf.a pdftops.o ../cups/$(LIBCUPS)
	echo Linking $@...
	$(CXX) $(LDFLAGS) -o $@ pdftops.o libxpdf.a $(LIBS) $(CXXLIBS) -lm


#
# libxpdf.a
#

libxpdf.a:	$(LIBOBJS)
	echo Archiving $@...
	$(RM) $@
	$(AR) $(ARFLAGS) $@ $(LIBOBJS)
	$(RANLIB) $@


#
# Dependencies...
#
  
include Dependencies


#
# End of "$Id: Makefile,v 1.6.2.4 2002/06/04 20:32:55 mike Exp $".
#
