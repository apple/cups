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

typedef void (*FontFileOutputFunc)(void *stream, const char *data, int len);

//------------------------------------------------------------------------
// FontFile
//------------------------------------------------------------------------

class FontFile {
public:

  FontFile();
  virtual ~FontFile();

  // Returns the font name, as specified internally by the font file.
  // Returns NULL if no name is available.
  virtual const char *getName() = 0;

  // Returns the custom font encoding, or NULL if the encoding is not
  // available.
  virtual const char **getEncoding() = 0;
};

//------------------------------------------------------------------------
// Type1FontFile
//------------------------------------------------------------------------

class Type1FontFile: public FontFile {
public:

  Type1FontFile(const char *file, int len);
  virtual ~Type1FontFile();
  virtual const char *getName() { return name; }
  virtual const char **getEncoding() { return (const char **)encoding; }

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

  Type1CFontFile(const char *fileA, int lenA);
  virtual ~Type1CFontFile();
  GBool isOk() { return ok; }

  virtual const char *getName();
  virtual const char **getEncoding();

  // Convert to a Type 1 font, suitable for embedding in a PostScript
  // file.  The name will be used as the PostScript font name.
  void convertToType1(FontFileOutputFunc outputFuncA, void *outputStreamA);

  // Convert to a Type 0 CIDFont, suitable for embedding in a
  // PostScript file.  The name will be used as the PostScript font
  // name.
  void convertToCIDType0(const char *psName,
			 FontFileOutputFunc outputFuncA, void *outputStreamA);

  // Convert to a Type 0 (but non-CID) composite font, suitable for
  // embedding in a PostScript file.  The name will be used as the
  // PostScript font name.
  void convertToType0(const char *psName,
		      FontFileOutputFunc outputFuncA, void *outputStreamA);

private:

  void readEncoding();
  void readTopDict(Type1CTopDict *dict);
  void readPrivateDict(Type1CPrivateDict *privateDict,
		       int offset, int size);
  Gushort *readCharset(int charset, int nGlyphs);
  void eexecWrite(const char *s);
  void eexecCvtGlyph(const char *glyphName, int pos, int n);
  void cvtGlyph(int pos, int n, GBool top);
  void cvtGlyphWidth(GBool useOp);
  void eexecDumpNum(double x, GBool fpA);
  void eexecDumpOp1(int opA);
  void eexecDumpOp2(int opA);
  void eexecWriteCharstring(Guchar *s, int n);
  void getDeltaInt(char *buf, const char *key, double *opA, int n);
  void getDeltaReal(char *buf, const char *key, double *opA, int n);
  int getIndexLen(int indexPos);
  int getIndexValPos(int indexPos, int i, int *valLen);
  int getIndexEnd(int indexPos);
  Guint getWord(int pos, int size);
  double getNum(int *pos, GBool *fp);
  char *getString(int sid, char *buf);

  const Guchar *file;
  int len;

  GString *name;
  char **encoding;

  int topDictIdxPos;
  int stringIdxPos;
  int gsubrIdxPos;
  int subrIdxPos;
  int gsubrBias;
  int subrBias;

  FontFileOutputFunc outputFunc;
  void *outputStream;
  double op[48];		// operands
  GBool fp[48];			// true if operand is fixed point
  int nOps;			// number of operands
  int nHints;			// number of hints for the current glyph
  GBool firstOp;		// true if we haven't hit the first op yet
  double defaultWidthX;		// default glyph width
  double nominalWidthX;		// nominal glyph width
  GBool defaultWidthXFP;	// true if defaultWidthX is fixed point
  GBool nominalWidthXFP;	// true if nominalWidthX is fixed point
  Gushort r1;			// eexec encryption key
  GString *charBuf;		// charstring output buffer
  int line;			// number of eexec chars on current line

  GBool ok;
};

//------------------------------------------------------------------------
// TrueTypeFontFile
//------------------------------------------------------------------------

struct TTFontTableHdr;

class TrueTypeFontFile: public FontFile {
public:

  TrueTypeFontFile(const char *fileA, int lenA);
  ~TrueTypeFontFile();

  // This always returns NULL, since it's probably better to trust the
  // font name in the PDF file rather than the one in the TrueType
  // font file.
  virtual const char *getName();

  virtual const char **getEncoding();

  // Convert to a Type 42 font, suitable for embedding in a PostScript
  // file.  The name will be used as the PostScript font name (so we
  // don't need to depend on the 'name' table in the font).  The
  // encoding is needed because the PDF Font object can modify the
  // encoding.
  void convertToType42(const char *name, const char **encodingA,
		       CharCodeToUnicode *toUnicode,
		       GBool pdfFontHasEncoding,
		       GBool pdfFontIsSymbolic,
		       FontFileOutputFunc outputFunc, void *outputStream);

  // Convert to a Type 2 CIDFont, suitable for embedding in a
  // PostScript file.  The name will be used as the PostScript font
  // name (so we don't need to depend on the 'name' table in the
  // font).
  void convertToCIDType2(const char *name, Gushort *cidMap, int nCIDs,
			 FontFileOutputFunc outputFunc, void *outputStream);

  // Convert to a Type 0 (but non-CID) composite font, suitable for
  // embedding in a PostScript file.  The name will be used as the
  // PostScript font name (so we don't need to depend on the 'name'
  // table in the font).
  void convertToType0(const char *name, Gushort *cidMap, int nCIDs,
		      FontFileOutputFunc outputFunc, void *outputStream);

  // Write a TTF file, filling in any missing tables that are required
  // by the TrueType spec.  If the font already has all the required
  // tables, it will be written unmodified.
  void writeTTF(FILE *out);

private:

  const char *file;
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
  int seekTable(const char *tag);
  int seekTableIdx(const char *tag);
  void cvtEncoding(const char **encodingA, GBool pdfFontHasEncoding,
		   FontFileOutputFunc outputFunc, void *outputStream);
  void cvtCharStrings(const char **encodingA, CharCodeToUnicode *toUnicode,
		      GBool pdfFontHasEncoding, GBool pdfFontIsSymbolic,
		      FontFileOutputFunc outputFunc, void *outputStream);
  int getCmapEntry(int cmapFmt, int pos, int code);
  void cvtSfnts(FontFileOutputFunc outputFunc, void *outputStream,
		GString *name);
  void dumpString(const char *s, int length,
		  FontFileOutputFunc outputFunc, void *outputStream);
  Guint computeTableChecksum(const char *data, int length);
};

#endif
