//========================================================================
//
// GfxFont.h
//
// Copyright 1996 Derek B. Noonburg
//
//========================================================================

#ifndef GFXFONT_H
#define GFXFONT_H

#ifdef __GNUC__
#pragma interface
#endif

#include "gtypes.h"
#include "GString.h"
#include "Object.h"
#include "FontEncoding.h"

class Dict;
struct BuiltinFont;

//------------------------------------------------------------------------
// GfxFontCharSet16
//------------------------------------------------------------------------

enum GfxFontCharSet16 {
  font16AdobeJapan12			// Adobe-Japan1-2
};

//------------------------------------------------------------------------
// GfxFontEncoding16
//------------------------------------------------------------------------

struct GfxFontEncoding16 {
  int wMode;			// writing mode (0=horizontal, 1=vertical)
  Guchar codeLen[256];		// length of codes, in bytes, indexed by
				//   first byte of code
  Gushort map1[256];		// one-byte code mapping:
				//   map1[code] --> 16-bit char selector
  Gushort *map2;		// two-byte code mapping
				//   map2[2*i]   --> first code in range
				//   map2[2*i+1] --> 16-bit char selector
				//                   for map2[2*i]
  int map2Len;			// length of map2 array (divided by 2)
};

//------------------------------------------------------------------------
// GfxFontWidths16
//------------------------------------------------------------------------

struct GfxFontWidthExcep {
  int first;			// this record applies to
  int last;			//   chars <first>..<last>
  double width;			// char width
};

struct GfxFontWidthExcepV {
  int first;			// this record applies to
  int last;			//   chars <first>..<last>
  double height;		// char height
  double vx, vy;		// origin position
};

struct GfxFontWidths16 {
  double defWidth;		// default char width
  double defHeight;		// default char height
  double defVY;			// default origin position
  GfxFontWidthExcep *exceps;	// exceptions
  int numExceps;		// number of valid entries in exceps
  GfxFontWidthExcepV *excepsV;	// exceptions for vertical font
  int numExcepsV;		// number of valid entries in excepsV
};

//------------------------------------------------------------------------
// GfxFont
//------------------------------------------------------------------------

#define fontFixedWidth (1 << 0)
#define fontSerif      (1 << 1)
#define fontSymbolic   (1 << 2)
#define fontItalic     (1 << 6)
#define fontBold       (1 << 18)

enum GfxFontType {
  fontUnknownType,
  fontType1,
  fontType1C,
  fontType3,
  fontTrueType,
  fontType0
};

class GfxFont {
public:

  // Constructor.
  GfxFont(char *tag1, Ref id1, Dict *fontDict);

  // Destructor.
  ~GfxFont();

  // Get font tag.
  GString *getTag() { return tag; }

  // Get font dictionary ID.
  Ref getID() { return id; }

  // Does this font match the tag?
  GBool matches(char *tag1) { return !tag->cmp(tag1); }

  // Get base font name.
  GString *getName() { return name; }

  // Get font type.
  GfxFontType getType() { return type; }

  // Does this font use 16-bit characters?
  GBool is16Bit() { return is16; }

  // Get embedded font ID, i.e., a ref for the font file stream.
  // Returns false if there is no embedded font.
  GBool getEmbeddedFontID(Ref *embID)
    { *embID = embFontID; return embFontID.num >= 0; }

  // Get the PostScript font name for the embedded font.  Returns
  // NULL if there is no embedded font.
  char *getEmbeddedFontName()
    { return embFontName ? embFontName->getCString() : (char *)NULL; }

  // Get the name of the external font file.  Returns NULL if there
  // is no external font file.
  char *getExtFontFile()
    { return extFontFile ? extFontFile->getCString() : (char *)NULL; }

  // Get font descriptor flags.
  GBool isFixedWidth() { return flags & fontFixedWidth; }
  GBool isSerif() { return flags & fontSerif; }
  GBool isSymbolic() { return flags & fontSymbolic; }
  GBool isItalic() { return flags & fontItalic; }
  GBool isBold() { return flags & fontBold; }

  // Get width of a character or string.
  double getWidth(Guchar c) { return widths[c]; }
  double getWidth(GString *s);

  // Get character metrics for 16-bit font.
  double getWidth16(int c);
  double getHeight16(int c);
  double getOriginX16(int c);
  double getOriginY16(int c);

  // Return the encoding.
  FontEncoding *getEncoding() { return encoding; }

  // Return the character name associated with <code>.
  char *getCharName(int code) { return encoding->getCharName(code); }

  // Return the code associated with <name>.
  int getCharCode(char *charName) { return encoding->getCharCode(charName); }

  // Return the 16-bit character set and encoding.
  GfxFontCharSet16 getCharSet16() { return enc16.charSet; }
  GfxFontEncoding16 *getEncoding16() { return enc16.enc; }

  // Get the writing mode (0=horizontal, 1=vertical).
  int getWMode16() { return enc16.enc->wMode; }

  // Return the font matrix.
  double *getFontMatrix() { return fontMat; }

  // Read an external or embedded font file into a buffer.
  char *readExtFontFile(int *len);
  char *readEmbFontFile(int *len);

private:

  void getEncAndWidths(Dict *fontDict, BuiltinFont *builtinFont);
  void findExtFontFile();
  void makeWidths(Dict *fontDict, FontEncoding *builtinEncoding,
		  Gushort *builtinWidths);
  void getType0EncAndWidths(Dict *fontDict);

  GString *tag;			// PDF font tag
  Ref id;			// reference (used as unique ID)
  GString *name;		// font name
  int flags;			// font descriptor flags
  GfxFontType type;		// type of font
  GBool is16;			// set if font uses 16-bit chars
  GString *embFontName;		// name of embedded font
  Ref embFontID;		// ref to embedded font file stream
  GString *extFontFile;		// external font file name
  double fontMat[6];		// font matrix
  union {
    FontEncoding *encoding;	// 8-bit font encoding
    struct {
      GfxFontCharSet16 charSet;	// 16-bit character set
      GfxFontEncoding16 *enc;	// 16-bit encoding (CMap)
    } enc16;
  };
  union {
    double widths[256];		// width of each char for 8-bit font
    GfxFontWidths16 widths16;	// char widths for 16-bit font
  };
};

//------------------------------------------------------------------------
// GfxFontDict
//------------------------------------------------------------------------

class GfxFontDict {
public:

  // Build the font dictionary, given the PDF font dictionary.
  GfxFontDict(Dict *fontDict);

  // Destructor.
  ~GfxFontDict();

  // Get the specified font.
  GfxFont *lookup(char *tag);

  // Iterative access.
  int getNumFonts() { return numFonts; }
  GfxFont *getFont(int i) { return fonts[i]; }

private:

  GfxFont **fonts;		// list of fonts
  int numFonts;			// number of fonts
};

#endif
