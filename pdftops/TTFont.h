//========================================================================
//
// TTFont.h
//
// An X wrapper for the FreeType TrueType font rasterizer.
//
// Copyright 2001-2002 Glyph & Cog, LLC
//
//========================================================================

#ifndef TTFONT_H
#define TTFONT_H

#if !FREETYPE2 && (HAVE_FREETYPE_FREETYPE_H || HAVE_FREETYPE_H)

#ifdef __GNUC__
#pragma interface
#endif

#if HAVE_FREETYPE_FREETYPE_H
#include <freetype/freetype.h>
#include <freetype/ftxpost.h>
#else
#include <freetype.h>
#include <ftxpost.h>
#endif
#include "gtypes.h"
#include "SFont.h"

//------------------------------------------------------------------------

class TTFontEngine: public SFontEngine {
public:

  TTFontEngine(Display *displayA, Visual *visualA, int depthA,
	       Colormap colormapA, GBool aaA);
  GBool isOk() { return ok; }
  virtual ~TTFontEngine();

private:

  TT_Engine engine;
  GBool aa;
  Gulong palette[5];
  GBool ok;

  friend class TTFontFile;
  friend class TTFont;
};

//------------------------------------------------------------------------

enum TTFontIndexMode {
  ttFontModeUnicode,
  ttFontModeCharCode,
  ttFontModeCharCodeOffset,
  ttFontModeCodeMap,
  ttFontModeCIDToGIDMap
};

class TTFontFile: public SFontFile {
public:

  // 8-bit font, TrueType or Type 1/1C
  TTFontFile(TTFontEngine *engineA, char *fontFileName,
	     char **fontEnc, GBool pdfFontHasEncoding);

  // CID font, TrueType
  TTFontFile(TTFontEngine *engineA, char *fontFileName,
	     Gushort *cidToGIDA, int cidToGIDLenA);

  GBool isOk() { return ok; }
  virtual ~TTFontFile();

private:

  TTFontEngine *engine;
  TT_Face face;
  TT_CharMap charMap;
  TTFontIndexMode mode;
  int charMapOffset;
  Guchar *codeMap;
  Gushort *cidToGID;
  int cidToGIDLen;
  GBool ok;

  friend class TTFont;
};

//------------------------------------------------------------------------

struct TTFontCacheTag {
  Gushort code;
  Gushort mru;			// valid bit (0x8000) and MRU index
};

class TTFont: public SFont {
public:

  TTFont(TTFontFile *fontFileA, double *m);
  GBool isOk() { return ok; }
  virtual ~TTFont();
  virtual GBool drawChar(Drawable d, int w, int h, GC gc,
			 int x, int y, int r, int g, int b,
			 CharCode c, Unicode u);

private:

  GBool getGlyphPixmap(CharCode c, Unicode u);

  TTFontFile *fontFile;
  TT_Instance instance;
  TT_Glyph glyph;
  TT_Raster_Map ras;
  XImage *image;
  TT_Matrix matrix;
  TT_F26Dot6 xOffset, yOffset;
  Guchar *cache;		// glyph pixmap cache
  TTFontCacheTag *cacheTags;	// cache tags, i.e., char codes
  int cacheSets;		// number of sets in cache
  int cacheAssoc;		// cache associativity (glyphs per set)
  GBool ok;
};

#endif // !FREETYPE2 && (HAVE_FREETYPE_FREETYPE_H || HAVE_FREETYPE_H)

#endif
