//========================================================================
//
// TTFont.h
//
// An X wrapper for the FreeType TrueType font rasterizer.
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

class TTFontFile: public SFontFile {
public:

  TTFontFile(TTFontEngine *engineA, char *fontFileName);
  GBool isOk() { return ok; }
  virtual ~TTFontFile();

private:

  TTFontEngine *engine;
  TT_Face face;
  TT_CharMap charMap;
  int charMapOffset;
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
			 int x, int y, int r, int g, int b, Gushort c);

private:

  GBool getGlyphPixmap(Gushort c);

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
