//========================================================================
//
// FontFile.h
//
// Copyright 1999-2003 Glyph & Cog, LLC
//
//========================================================================

#ifndef FONTFILE_H
#define FONTFILE_H

#include <config.h>

#ifdef USE_GCC_PRAGMAS
#pragma interface
#endif

#include <stdio.h>
#include "gtypes.h"
#include "GString.h"
#include "CharTypes.h"

class CharCodeToUnicode;

//------------------------------------------------------------------------

typedef void (*FontFileOutputFunc)(void *stream, char *data, int len);

//------------------------------------------------------------------------
// FontFile
//------------------------------------------------------------------------

class FontFile {
public:

  FontFile();
  virtual ~FontFile();

  // Returns the font name, as specified internally by the font file.
  // Returns NULL if no name is available.
  virtual char *getName() = 0;

  // Returns the custom font encoding, or NULL if the encoding is not
  // available.
  virtual char **getEncoding() = 0;
};

//------------------------------------------------------------------------
// Type1FontFile
//------------------------------------------------------------------------

class Type1FontFile: public FontFile {
public:

  Type1FontFile(char *file, int len);
  virtual ~Type1FontFile();
  virtual char *getName() { return name; }
  virtual char **getEncoding() { return encoding; }

private:

  char *name;
  char **encoding;
};

//------------------------------------------------------------------------
// Type1CFontFile
//------------------------------------------------------------------------

struct Type1CTopDict;
struct Type1CPrivateDict;

class Type1CFontFile: public FontFile {
public:

  Type1CFontFile(char *fileA, int lenA);
  virtual ~Type1CFontFile();

  virtual char *getName();
  virtual char **getEncoding();

  // Convert to a Type 1 font, suitable for embedding in a PostScript
  // file.  The name will be used as the PostScript font name.
  void convertToType1(FontFileOutputFunc outputFuncA, void *outputStreamA);

  // Convert to a Type 0 CIDFont, suitable for embedding in a
  // PostScript file.  The name will be used as the PostScript font
  // name.
  void convertToCIDType0(char *psName,
			 FontFileOutputFunc outputFuncA, void *outputStreamA);

  // Convert to a Type 0 (but non-CID) composite font, suitable for
  // embedding in a PostScript file.  The name will be used as the
  // PostScript font name.
  void convertToType0(char *psName,
		      FontFileOutputFunc outputFuncA, void *outputStreamA);

private:

  void readNameAndEncoding();
  void readTopDict(Type1CTopDict *dict);
  void readPrivateDict(Type1CPrivateDict *privateDict,
		       int offset, int size);
  Gushort *readCharset(int charset, int nGlyphs);
  void eexecWrite(char *s);
  void eexecCvtGlyph(char *glyphName, Guchar *s, int n);
  void cvtGlyph(Guchar *s, int n);
  void cvtGlyphWidth(GBool useOp);
  void eexecDumpNum(double x, GBool fpA);
  void eexecDumpOp1(int opA);
  void eexecDumpOp2(int opA);
  void eexecWriteCharstring(Guchar *s, int n);
  void getDeltaInt(char *buf, char *key, double *opA, int n);
  void getDeltaReal(char *buf, char *key, double *opA, int n);
  int getIndexLen(Guchar *indexPtr);
  Guchar *getIndexValPtr(Guchar *indexPtr, int i);
  Guchar *getIndexEnd(Guchar *indexPtr);
  Guint getWord(Guchar *ptr, int size);
  double getNum(Guchar **ptr, GBool *fp);
  char *getString(int sid, char *buf);

  char *file;
  int len;

  GString *name;
  char **encoding;

  int topOffSize;
  Guchar *topDictIdxPtr;
  Guchar *stringIdxPtr;
  Guchar *gsubrIdxPtr;

  FontFileOutputFunc outputFunc;
  void *outputStream;
  double op[48];		// operands
  GBool fp[48];			// true if operand is fixed point
  int nOps;			// number of operands
  double defaultWidthX;		// default glyph width
  double nominalWidthX;		// nominal glyph width
  GBool defaultWidthXFP;	// true if defaultWidthX is fixed point
  GBool nominalWidthXFP;	// true if nominalWidthX is fixed point
  Gushort r1;			// eexec encryption key
  GString *charBuf;		// charstring output buffer
  int line;			// number of eexec chars on current line
};

//------------------------------------------------------------------------
// TrueTypeFontFile
//------------------------------------------------------------------------

struct TTFontTableHdr;

class TrueTypeFontFile: public FontFile {
public:

  TrueTypeFontFile(char *fileA, int lenA);
  ~TrueTypeFontFile();

  // This always returns NULL, since it's probably better to trust the
  // font name in the PDF file rather than the one in the TrueType
  // font file.
  virtual char *getName();

  virtual char **getEncoding();

  // Convert to a Type 42 font, suitable for embedding in a PostScript
  // file.  The name will be used as the PostScript font name (so we
  // don't need to depend on the 'name' table in the font).  The
  // encoding is needed because the PDF Font object can modify the
  // encoding.
  void convertToType42(char *name, char **encodingA,
		       CharCodeToUnicode *toUnicode,
		       GBool pdfFontHasEncoding,
		       FontFileOutputFunc outputFunc, void *outputStream);

  // Convert to a Type 2 CIDFont, suitable for embedding in a
  // PostScript file.  The name will be used as the PostScript font
  // name (so we don't need to depend on the 'name' table in the
  // font).
  void convertToCIDType2(char *name, Gushort *cidMap, int nCIDs,
			 FontFileOutputFunc outputFunc, void *outputStream);

  // Convert to a Type 0 (but non-CID) composite font, suitable for
  // embedding in a PostScript file.  The name will be used as the
  // PostScript font name (so we don't need to depend on the 'name'
  // table in the font).
  void convertToType0(char *name, Gushort *cidMap, int nCIDs,
		      FontFileOutputFunc outputFunc, void *outputStream);

  // Write a TTF file, filling in any missing tables that are required
  // by the TrueType spec.  If the font already has all the required
  // tables, it will be written unmodified.
  void writeTTF(FILE *out);

private:

  char *file;
  int len;

  char **encoding;

  TTFontTableHdr *tableHdrs;
  int nTables;
  int bbox[4];
  int locaFmt;
  int nGlyphs;
  GBool mungedCmapSize;

  int getByte(int pos);
  int getChar(int pos);
  int getUShort(int pos);
  int getShort(int pos);
  Guint getULong(int pos);
  double getFixed(int pos);
  int seekTable(char *tag);
  int seekTableIdx(char *tag);
  void cvtEncoding(char **encodingA, GBool pdfFontHasEncoding,
		   FontFileOutputFunc outputFunc, void *outputStream);
  void cvtCharStrings(char **encodingA, CharCodeToUnicode *toUnicode,
		      GBool pdfFontHasEncoding,
		      FontFileOutputFunc outputFunc, void *outputStream);
  int getCmapEntry(int cmapFmt, int pos, int code);
  void cvtSfnts(FontFileOutputFunc outputFunc, void *outputStream,
		GString *name);
  void dumpString(char *s, int length,
		  FontFileOutputFunc outputFunc, void *outputStream);
  Guint computeTableChecksum(char *data, int length);
};

#endif
