//========================================================================
//
// FontFile.h
//
// Copyright 1999 Derek B. Noonburg
//
//========================================================================

#ifndef FONTFILE_H
#define FONTFILE_H

#ifdef __GNUC__
#pragma interface
#endif

#include <stdio.h>
#include "gtypes.h"
#include "GString.h"
#include "FontEncoding.h"

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

  // Returns the custom font encoding, or NULL if the encoding is
  // not available.  If <taken> is set, the caller of this function
  // will be responsible for freeing the encoding object.
  virtual FontEncoding *getEncoding(GBool taken) = 0;
};

//------------------------------------------------------------------------
// Type1FontFile
//------------------------------------------------------------------------

class Type1FontFile: public FontFile {
public:

  Type1FontFile(const char *file, int len);
  virtual ~Type1FontFile();
  virtual const char *getName() { return name; }
  virtual FontEncoding *getEncoding(GBool taken);

private:

  const char *name;
  FontEncoding *encoding;
  GBool freeEnc;
};

//------------------------------------------------------------------------
// Type1CFontFile
//------------------------------------------------------------------------

class Type1CFontFile: public FontFile {
public:

  Type1CFontFile(const char *file, int len);
  virtual ~Type1CFontFile();
  virtual const char *getName() { return name; }
  virtual FontEncoding *getEncoding(GBool taken);

private:

  const char *name;
  FontEncoding *encoding;
  GBool freeEnc;
};

//------------------------------------------------------------------------
// Type1CFontConverter
//------------------------------------------------------------------------

class Type1CFontConverter {
public:

  Type1CFontConverter(const char *file, int len, FILE *out);
  ~Type1CFontConverter();
  void convert();

private:

  void eexecWrite(const char *s);
  void cvtGlyph(const char *name, Guchar *s, int n);
  void cvtGlyphWidth(GBool useOp);
  void eexecDumpNum(double x, GBool fp);
  void eexecDumpOp1(int op);
  void eexecDumpOp2(int op);
  void eexecWriteCharstring(Guchar *s, int n);
  void getDeltaInt(char *buf, const char *name, double *op, int n);
  void getDeltaReal(char *buf, const char *name, double *op, int n);

  const char *file;
  int len;
  FILE *out;
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

  TrueTypeFontFile(const char *file, int len);
  ~TrueTypeFontFile();

  // This always returns NULL, since it's probably better to trust the
  // font name in the PDF file rather than the one in the TrueType
  // font file.
  virtual const char *getName();

  virtual FontEncoding *getEncoding(GBool taken);

  // Convert to a Type 42 font, suitable for embedding in a PostScript
  // file.  The name will be used as the PostScript font name (so we
  // don't need to depend on the 'name' table in the font).  The
  // encoding is needed because the PDF Font object can modify the
  // encoding.
  void convertToType42(const char *name, FontEncoding *encoding, FILE *out);

private:

  const char *file;
  int len;

  FontEncoding *encoding;
  GBool freeEnc;

  TTFontTableHdr *tableHdrs;
  int nTables;
  int bbox[4];
  int locaFmt;
  int nGlyphs;

  int getByte(int pos);
  int getChar(int pos);
  int getUShort(int pos);
  int getShort(int pos);
  Guint getULong(int pos);
  double getFixed(int pos);
  int seekTable(const char *tag);
  void cvtEncoding(FontEncoding *encoding, FILE *out);
  void cvtCharStrings(FontEncoding *encoding, FILE *out);
  void cvtSfnts(FILE *out);
  void dumpString(const char *s, int len, FILE *out);
};

#endif
