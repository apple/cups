#
# "$Id: Makefile,v 1.6.2.5 2002/09/18 17:03:20 mike Exp $"
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

LIBOBJS	=	Annot.o \
		Array.o \
		BuiltinFont.o \
		BuiltinFontTables.o \
		CMap.o \
		Catalog.o \
		CharCodeToUnicode.o \
		Decrypt.o \
		Dict.o \
		Error.o \
		FTFont.o \
		FontEncoding.o \
		FontEncodingTables.o \
		FontFile.o \
		Function.o \
		GHash.o \
		GList.o \
		GString.o \
		Gfx.o \
		GfxFont.o \
		GfxState.o \
		GlobalParams.o \
		Lexer.o \
		Link.o \
		NameToCharCode.o \
		Object.o \
		OutputDev.o \
		PDFDoc.o \
		PSOutputDev.o \
		PSTokenizer.o \
		Page.o \
		Params.o \
		Parser.o \
		SFont.o \
		Stream.o \
		T1Font.o \
		TTFont.o \
		UnicodeMap.o \
		XRef.o \
		gfile.o \
		gmem.o \
		gmempp.o \
		parseargs.o

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
# End of "$Id: Makefile,v 1.6.2.5 2002/09/18 17:03:20 mike Exp $".
#
