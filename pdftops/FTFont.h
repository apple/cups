//========================================================================
//
// FTFont.h
//
// An X wrapper for the FreeType font rasterizer.
//
// Copyright 2001-2002 Glyph & Cog, LLC
//
//========================================================================

#ifndef FTFONT_H
#define FTFONT_H

#if FREETYPE2 && (HAVE_FREETYPE_FREETYPE_H || HAVE_FREETYPE_H)

#ifdef __GNUC__
#pragma interface
#endif

#include <freetype/freetype.h>
#include "CharTypes.h"
#include "SFont.h"

//------------------------------------------------------------------------

class FTFontEngine: public SFontEngine {
public:

  FTFontEngine(Display *displayA, Visual *visualA, int depthA,
	       Colormap colormapA, GBool aaA);
  GBool isOk() { return ok; }
  virtual ~FTFontEngine();

private:

  FT_Library lib;
  GBool aa;
  Gulong palette[5];
  GBool ok;

  friend class FTFontFile;
  friend class FTFont;
};

//------------------------------------------------------------------------

enum FTFontIndexMode {
  ftFontModeUnicode,
  ftFontModeCharCode,
  ftFontModeCharCodeOffset,
  ftFontModeCodeMap,
  ftFontModeCodeMapDirect,
  ftFontModeCIDToGIDMap,
  ftFontModeCFFCharset
};

class FTFontFile: public SFontFile {
public:

  // 8-bit font, TrueType or Type 1/1C
  FTFontFile(FTFontEngine *engineA, char *fontFileName,
	     char **fontEnc, GBool pdfFontHasEncoding);

  // CID font, TrueType
  FTFontFile(FTFontEngine *engineA, char *fontFileName,
	     Gushort *cidToGIDA, int cidToGIDLenA);

  // CID font, Type 0C (CFF)
  FTFontFile(FTFontEngine *engineA, char *fontFileName);

  GBool isOk() { return ok; }
  virtual ~FTFontFile();

private:

  FTFontEngine *engine;
  FT_Face face;
  FTFontIndexMode mode;
  int charMapOffset;
  Guint *codeMap;
  Gushort *cidToGID;
  int cidToGIDLen;
  GBool ok;

  friend class FTFont;
};

//------------------------------------------------------------------------

struct FTFontCacheTag {
  Gushort code;
  Gushort mru;			// valid bit (0x8000) and MRU index
  int x, y, w, h;		// offset and size of glyph
};

class FTFont: public SFont {
public:

  FTFont(FTFontFile *fontFileA, double *m);
  GBool isOk() { return ok; }
  virtual ~FTFont();
  virtual GBool drawChar(Drawable d, int w, int h, GC gc,
			 int x, int y, int r, int g, int b,
			 CharCode c, Unicode u);
  virtual GBool getCharPath(CharCode c, Unicode u, GfxState *state);

private:

  Guchar *getGlyphPixmap(CharCode c, Unicode u,
			 int *x, int *y, int *w, int *h);
  static int charPathMoveTo(FT_Vector *pt, void *state);
  static int charPathLineTo(FT_Vector *pt, void *state);
  static int charPathConicTo(FT_Vector *ctrl, FT_Vector *pt, void *state);
  static int charPathCubicTo(FT_Vector *ctrl1, FT_Vector *ctrl2,
			     FT_Vector *pt, void *state);
  FT_UInt getGlyphIndex(CharCode c, Unicode u);

  FTFontFile *fontFile;
  FT_Size sizeObj;
  XImage *image;
  FT_Matrix matrix;
  int glyphW, glyphH;		// size of glyph pixmaps
  int glyphSize;		// size of glyph pixmaps, in bytes
  Guchar *cache;		// glyph pixmap cache
  FTFontCacheTag *cacheTags;	// cache tags, i.e., char codes
  int cacheSets;		// number of sets in cache
  int cacheAssoc;		// cache associativity (glyphs per set)
  GBool ok;
};

#endif // FREETYPE2 && (HAVE_FREETYPE_FREETYPE_H || HAVE_FREETYPE_H)

#endif
