#
# "$Id: Makefile,v 1.2 2000/07/07 17:02:45 mike Exp $"
#
#   pdftops filter Makefile for the Common UNIX Printing System (CUPS).
#
#   CUPS filter changes Copyright 2000 by Easy Software Products.
#   Xpdf code Copyright 1996-1999 by Derek B. Noonburg
#


include ../Makedefs

#
# Object files...
#

LIBOBJS	=	GString.o gfile.o gmempp.o gmem.o parseargs.o \
		Array.o Catalog.o Dict.o Error.o FontEncoding.o \
		FontFile.o Gfx.o GfxFont.o GfxState.o Lexer.o Link.o \
		Object.o OutputDev.o Page.o Params.o Parser.o PDFDoc.o \
		PSOutputDev.o Stream.o XRef.o
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
	$(RM) pdftops


#
# Install the filter...
#

install:
	$(MKDIR) $(SERVERBIN)/filter
	$(CHMOD) ugo+rx $(SERVERBIN)
	$(CHMOD) ugo+rx $(SERVERBIN)/filter
	$(RM) $(SERVERBIN)/filter/pdftops
	$(INSTALL_BIN) pdftops $(SERVERBIN)/filter


#
# pdftops
#

pdftops:	libxpdf.a pdftops.o ../cups/$(LIBCUPS)
	echo Linking $@...
	$(CXX) $(LDFLAGS) -o $@ pdftops.o libxpdf.a $(LIBS) -lm

pdftops.o:	parseargs.h gtypes.h GString.h gmem.h Object.h Array.h \
		Dict.h Stream.h XRef.h Catalog.h Page.h PDFDoc.h Link.h \
		PSOutputDev.h config.h OutputDev.h Params.h Error.h


#
# libxpdf.a
#

libxpdf.a:	$(LIBOBJS)
	echo Archiving $@...
	$(RM) $@
	$(AR) $(ARFLAGS) $@ $(LIBOBJS)
	$(RANLIB) $@

GString.o:	GString.h
gmempp.o:	gmem.h
gfile.o:	GString.h gfile.h gtypes.h
gmem.o:		gmem.h
parseargs.o:	parseargs.h gtypes.h
Array.o:	gmem.h Object.h gtypes.h GString.h Array.h Dict.h Stream.h
Catalog.o:	gmem.h Object.h gtypes.h GString.h Array.h Dict.h Stream.h \
		Page.h Error.h config.h Link.h Catalog.h
Dict.o:		gmem.h Object.h gtypes.h GString.h Array.h Dict.h Stream.h \
		XRef.h
Error.o:	gtypes.h Params.h Error.h config.h
FontEncoding.o:	gmem.h FontEncoding.h gtypes.h
FontFile.o:	gmem.h Error.h config.h FontFile.h gtypes.h GString.h \
		FontEncoding.h StdFontInfo.h CompactFontInfo.h
Gfx.o:		gmem.h Object.h gtypes.h GString.h Array.h Dict.h Stream.h \
		Lexer.h Parser.h GfxFont.h FontEncoding.h GfxState.h \
		OutputDev.h Params.h Error.h config.h Gfx.h
GfxFont.o:	GString.h gmem.h gfile.h gtypes.h config.h Object.h Array.h \
		Dict.h Stream.h Error.h Params.h FontFile.h FontEncoding.h \
		GfxFont.h FontInfo.h
GfxState.o:	gmem.h Error.h config.h Object.h gtypes.h GString.h Array.h \
		Dict.h Stream.h GfxState.h
ImageOutputDev.o: gmem.h config.h Error.h GfxState.h gtypes.h Object.h \
		GString.h Array.h Dict.h Stream.h ImageOutputDev.h OutputDev.h
Lexer.o:	Lexer.h Object.h gtypes.h gmem.h GString.h Array.h Dict.h \
		Stream.h Error.h config.h
Link.o:		gmem.h GString.h Error.h config.h Object.h gtypes.h Array.h \
		Dict.h Stream.h Link.h
Object.o:	Object.h gtypes.h gmem.h GString.h Array.h Dict.h Stream.h \
		Error.h config.h XRef.h
OutputDev.o:	Object.h gtypes.h gmem.h GString.h Array.h Dict.h Stream.h \
		GfxState.h OutputDev.h
Page.o:		Object.h gtypes.h gmem.h GString.h Array.h Dict.h Stream.h \
		XRef.h OutputDev.h Gfx.h Error.h config.h Params.h Page.h
Params.o:	gtypes.h gmem.h GString.h gfile.h Params.h
Parser.o:	Object.h gtypes.h gmem.h GString.h Array.h Dict.h Stream.h \
		Parser.h Lexer.h Error.h config.h
PDFDoc.o:	GString.h config.h Page.h Object.h gtypes.h gmem.h Array.h \
		Dict.h Stream.h Catalog.h XRef.h Link.h OutputDev.h Params.h \
		Error.h PDFDoc.h
PSOutputDev.o:	GString.h config.h Object.h gtypes.h gmem.h Array.h Dict.h \
		Stream.h Error.h GfxState.h GfxFont.h FontEncoding.h \
		FontFile.h Catalog.h Page.h PSOutputDev.h OutputDev.h
Stream.o:	gmem.h config.h Error.h Object.h gtypes.h GString.h Array.h \
		Dict.h Stream.h Stream-CCITT.h
XRef.o:		gmem.h Object.h gtypes.h GString.h Array.h Dict.h Stream.h \
		Lexer.h Parser.h Error.h config.h XRef.h


$(OBJS):	../config.h ../Makedefs

#
# End of "$Id: Makefile,v 1.2 2000/07/07 17:02:45 mike Exp $".
#
