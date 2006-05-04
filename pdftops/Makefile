#
# "$Id$"
#
#   pdftops filter Makefile for the Common UNIX Printing System (CUPS).
#
#   CUPS filter changes Copyright 1997-2006 by Easy Software Products.
#   Xpdf code Copyright 1996-1999 by Derek B. Noonburg
#


include ../Makedefs

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
		FoFiBase.o \
		FoFiEncodings.o \
		FoFiTrueType.o \
		FoFiType1C.o \
		FoFiType1.o \
		FontEncodingTables.o \
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
		JArithmeticDecoder.o \
		JBIG2Stream.o \
		JPXStream.o \
		Lexer.o \
		Link.o \
		NameToCharCode.o \
		Object.o \
		Outline.o \
		OutputDev.o \
		Page.o \
		parseargs.o \
		Parser.o \
		PDFDoc.o \
		PDFDocEncoding.o \
		PSOutputDev.o \
		PSTokenizer.o \
		SecurityHandler.o \
		Stream.o \
		UnicodeMap.o \
		UnicodeTypeTable.o \
		XpdfPluginAPI.o \
		XRef.o

OBJS	=	pdftops.o $(LIBOBJS)

TARGETS	=	libxpdf.a pdftops


#
# Make everything...
#

all:	$(TARGETS)


#
# Clean all object files...
#

clean:
	$(RM) $(OBJS)
	$(RM) libxpdf.a
	$(RM) pdftops
	$(RM) pdftoraster


#
# Update dependencies (without system header dependencies...)
#

depend:
	makedepend -Y -I.. -fDependencies $(OBJS:.o=.cxx) gmem.c parseargs.c >/dev/null 2>&1


#
# Install the filter...
#

install:	all
	$(INSTALL_DIR) -m 755 $(SERVERBIN)/filter
	$(INSTALL_BIN) pdftops $(SERVERBIN)/filter


#
# Uninstall the filter...
#

uninstall:
	$(RM) $(SERVERBIN)/filter/pdftops
	-$(RMDIR) $(SERVERBIN)/filter
	-$(RMDIR) $(SERVERBIN)


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
# End of "$Id$".
#
