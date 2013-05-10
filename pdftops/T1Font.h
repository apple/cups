//========================================================================
//
// T1Font.h
//
// An X wrapper for the t1lib Type 1 font rasterizer.
//
// Copyright 2001-2002 Glyph & Cog, LLC
//
//========================================================================

#ifndef T1FONT_H
#define T1FONT_H

#if HAVE_T1LIB_H

#ifdef __GNUC__
#pragma interface
#endif

#include <X11/Xlib.h>
#include <t1lib.h>
#include "SFont.h"

class GfxState;

//------------------------------------------------------------------------

class T1FontEngine: public SFontEngine {
public:

  T1FontEngine(Display *displayA, Visual *visualA, int depthA,
	       Colormap colormapA, GBool aaA, GBool aaHighA);
  GBool isOk() { return ok; }
  virtual ~T1FontEngine();

private:

  GBool aa;			// use anti-aliasing?
  GBool aaHigh;			// use high-res anti-aliasing?
  GBool ok;

  friend class T1FontFile;
  friend class T1Font;
};

//------------------------------------------------------------------------

class T1FontFile: public SFontFile {
public:

  T1FontFile(T1FontEngine *engineA, char *fontFileName,
	     char **fontEnc, double *bboxA);
  GBool isOk() { return ok; }
  virtual ~T1FontFile();

private:

  T1FontEngine *engine;
  int id;			// t1lib font ID
  char **enc;
  char *encStr;
  double bbox[4];
  GBool ok;

  friend class T1Font;
};

//------------------------------------------------------------------------

struct T1FontCacheTag {
  Gushort code;
  Gushort mru;			// valid bit (0x8000) and MRU index
  int x, y, w, h;		// offset and size of glyph
};

class T1Font: public SFont {
public:

  T1Font(T1FontFile *fontFileA, double *m);
  GBool isOk() { return ok; }
  virtual ~T1Font();
  virtual GBool drawChar(Drawable d, int w, int h, GC gc,
			 int x, int y, int r, int g, int b,
			 CharCode c, Unicode u);
  virtual GBool getCharPath(CharCode c, Unicode u, GfxState *state);

private:

  Guchar *getGlyphPixmap(CharCode c, int *x, int *y, int *w, int *h);

  T1FontFile *fontFile;
  int id;
  float size;
  XImage *image;
  int glyphW, glyphH;		// size of glyph pixmaps
  int glyphSize;		// size of glyph pixmaps, in bytes
  Guchar *cache;		// glyph pixmap cache
  T1FontCacheTag *cacheTags;	// cache tags, i.e., char codes
  int cacheSets;		// number of sets in cache
  int cacheAssoc;		// cache associativity (glyphs per set)
  GBool ok;
};

#endif // HAVE_T1LIB_H

#endif
