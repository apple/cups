#
# "$Id: Makefile,v 1.6.2.9 2003/05/14 18:22:50 mike Exp $"
#
#   pdftops filter Makefile for the Common UNIX Printing System (CUPS).
#
#   CUPS filter changes Copyright 1997-2003 by Easy Software Products.
#   Xpdf code Copyright 1996-1999 by Derek B. Noonburg
#


include ../Makedefs

OPTIONS	=	-Wwrite-strings

#
# Object files...
#

LIBOBJS	=	\
		Annot.o \
		Array.o \
		BuiltinFont.o \
		BuiltinFontTables.o \
		Catalog.o \
		CharCodeToUnicode.o \
		CMap.o \
		Decrypt.o \
		Dict.o \
		Error.o \
		FontEncoding.o \
		FontEncodingTables.o \
		FontFile.o \
		Function.o \
		gfile.o \
		Gfx.o \
		GfxFont.o \
		GfxState.o \
		GHash.o \
		GList.o \
		GlobalParams.o \
		gmem.o \
		gmempp.o \
		GString.o \
		JBIG2Stream.o \
		Lexer.o \
		Link.o \
		NameToCharCode.o \
		Object.o \
		Outline.o \
		OutputDev.o \
		Page.o \
		Params.o \
		parseargs.o \
		Parser.o \
		PDFDoc.o \
		PDFDocEncoding.o \
		PSOutputDev.o \
		PSTokenizer.o \
		Stream.o \
		UnicodeMap.o \
		XRef.o


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
# End of "$Id: Makefile,v 1.6.2.9 2003/05/14 18:22:50 mike Exp $".
#
