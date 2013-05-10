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
  virtual char *getName() = 0;

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

  Type1FontFile(char *file, int len);
  virtual ~Type1FontFile();
  virtual char *getName() { return name; }
  virtual FontEncoding *getEncoding(GBool taken);

private:

  char *name;
  FontEncoding *encoding;
  GBool freeEnc;
};

//------------------------------------------------------------------------
// Type1CFontFile
//------------------------------------------------------------------------

class Type1CFontFile: public FontFile {
public:

  Type1CFontFile(char *file, int len);
  virtual ~Type1CFontFile();
  virtual char *getName() { return name; }
  virtual FontEncoding *getEncoding(GBool taken);

private:

  char *name;
  FontEncoding *encoding;
  GBool freeEnc;
};

//------------------------------------------------------------------------
// Type1CFontConverter
//------------------------------------------------------------------------

class Type1CFontConverter {
public:

  Type1CFontConverter(char *file, int len, FILE *out);
  ~Type1CFontConverter();
  void convert();

private:

  void eexecWrite(char *s);
  void cvtGlyph(char *name, Guchar *s, int n);
  void cvtGlyphWidth(GBool useOp);
  void eexecDumpNum(double x, GBool fp);
  void eexecDumpOp1(int op);
  void eexecDumpOp2(int op);
  void eexecWriteCharstring(Guchar *s, int n);
  void getDeltaInt(char *buf, char *name, double *op, int n);
  void getDeltaReal(char *buf, char *name, double *op, int n);

  char *file;
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

#endif
