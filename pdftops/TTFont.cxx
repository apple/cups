//========================================================================
//
// TTFont.cc
//
// Copyright 2001-2002 Glyph & Cog, LLC
//
//========================================================================

#ifdef __GNUC__
#pragma implementation
#endif

#include <config.h>

#if !FREETYPE2 && (HAVE_FREETYPE_FREETYPE_H || HAVE_FREETYPE_H)

#include <string.h>
#include "gmem.h"
#include "GlobalParams.h"
#include "TTFont.h"

//------------------------------------------------------------------------

TTFontEngine::TTFontEngine(Display *displayA, Visual *visualA, int depthA,
			   Colormap colormapA, GBool aaA):
  SFontEngine(displayA, visualA, depthA, colormapA) {
  static TT_Byte ttPalette[5] = {0, 1, 2, 3, 4};

  ok = gFalse;
  if (TT_Init_FreeType(&engine)) {
    return;
  }
  aa = aaA;
  if (aa) {
    if (TT_Set_Raster_Gray_Palette(engine, ttPalette)) {
      return;
    }
  }
  ok = gTrue;
}

TTFontEngine::~TTFontEngine() {
  TT_Done_FreeType(engine);
}

//------------------------------------------------------------------------

TTFontFile::TTFontFile(TTFontEngine *engineA, char *fontFileName,
		       char **fontEnc, GBool pdfFontHasEncoding) {
  TT_Face_Properties props;
  TT_UShort unicodeCmap, macRomanCmap, msSymbolCmap;
  TT_UShort platform, encoding, i;
  int j;

  ok = gFalse;
  engine = engineA;
  codeMap = NULL;
  if (TT_Open_Face(engine->engine, fontFileName, &face)) {
    return;
  }
  if (TT_Get_Face_Properties(face, &props)) {
    return;
  }

  // To match up with the Adobe-defined behaviour, we choose a cmap
  // like this:
  // 1. If the PDF font has an encoding:
  //    1a. If the TrueType font has a Microsoft Unicode cmap, use it,
  //        and use the Unicode indexes, not the char codes.
  //    1b. If the TrueType font has a Macintosh Roman cmap, use it,
  //        and reverse map the char names through MacRomanEncoding to
  //        get char codes.
  // 2. If the PDF font does not have an encoding:
  //    2a. If the TrueType font has a Macintosh Roman cmap, use it,
  //        and use char codes directly.
  //    2b. If the TrueType font has a Microsoft Symbol cmap, use it,
  //        and use (0xf000 + char code).
  // 3. If none of these rules apply, use the first cmap and hope for
  //    the best (this shouldn't happen).
  unicodeCmap = macRomanCmap = msSymbolCmap = 0xffff;
  for (i = 0; i < props.num_CharMaps; ++i) {
    if (!TT_Get_CharMap_ID(face, i, &platform, &encoding)) {
      if (platform == 3 && encoding == 1) {
	unicodeCmap = i;
      } else if (platform == 1 && encoding == 0) {
	macRomanCmap = i;
      } else if (platform == 3 && encoding == 0) {
	msSymbolCmap = i;
      }
    }
  }
  i = 0;
  mode = ttFontModeCharCode;
  charMapOffset = 0;
  if (pdfFontHasEncoding) {
    if (unicodeCmap != 0xffff) {
      i = unicodeCmap;
      mode = ttFontModeUnicode;
    } else if (macRomanCmap != 0xffff) {
      i = macRomanCmap;
      mode = ttFontModeCodeMap;
      codeMap = (Guchar *)gmalloc(256 * sizeof(Guchar));
      for (j = 0; j < 256; ++j) {
	if (fontEnc[j]) {
	  codeMap[j] = (Guchar)globalParams->getMacRomanCharCode(fontEnc[j]);
	} else {
	  codeMap[j] = 0;
	}
      }
    }
  } else {
    if (macRomanCmap != 0xffff) {
      i = macRomanCmap;
      mode = ttFontModeCharCode;
    } else if (msSymbolCmap != 0xffff) {
      i = msSymbolCmap;
      mode = ttFontModeCharCodeOffset;
      charMapOffset = 0xf000;
    }
  }
  TT_Get_CharMap(face, i, &charMap);

  ok = gTrue;
}

TTFontFile::TTFontFile(TTFontEngine *engineA, char *fontFileName,
		       Gushort *cidToGIDA, int cidToGIDLenA) {
  ok = gFalse;
  engine = engineA;
  codeMap = NULL;
  cidToGID = cidToGIDA;
  cidToGIDLen = cidToGIDLenA;
  if (TT_Open_Face(engine->engine, fontFileName, &face)) {
    return;
  }
  mode = ttFontModeCIDToGIDMap;
  ok = gTrue;
}

TTFontFile::~TTFontFile() {
  TT_Close_Face(face);
  if (codeMap) {
    gfree(codeMap);
  }
}

//------------------------------------------------------------------------

TTFont::TTFont(TTFontFile *fontFileA, double *m) {
  TTFontEngine *engine;
  TT_Face_Properties props;
  TT_Instance_Metrics metrics;
  int x, xMin, xMax;
  int y, yMin, yMax;
  int i;

  ok = gFalse;
  fontFile = fontFileA;
  engine = fontFile->engine;
  if (TT_New_Instance(fontFile->face, &instance) ||
      TT_Set_Instance_Resolutions(instance, 72, 72) ||
      TT_Set_Instance_CharSize(instance, 1000 * 64) ||
      TT_New_Glyph(fontFile->face, &glyph) ||
      TT_Get_Face_Properties(fontFile->face, &props) ||
      TT_Get_Instance_Metrics(instance, &metrics)) {
    return;
  }

  // transform the four corners of the font bounding box -- the min
  // and max values form the bounding box of the transformed font
  x = (int)((m[0] * props.header->xMin + m[2] * props.header->yMin) *
	    0.001 * metrics.x_ppem / props.header->Units_Per_EM);
  xMin = xMax = x;
  y = (int)((m[1] * props.header->xMin + m[3] * props.header->yMin) *
	    0.001 * metrics.x_ppem / props.header->Units_Per_EM);
  yMin = yMax = y;
  x = (int)((m[0] * props.header->xMin + m[2] * props.header->yMax) *
	    0.001 * metrics.x_ppem / props.header->Units_Per_EM);
  if (x < xMin) {
    xMin = x;
  } else if (x > xMax) {
    xMax = x;
  }
  y = (int)((m[1] * props.header->xMin + m[3] * props.header->yMax) *
	    0.001 * metrics.x_ppem / props.header->Units_Per_EM);
  if (y < yMin) {
    yMin = y;
  } else if (y > yMax) {
    yMax = y;
  }
  x = (int)((m[0] * props.header->xMax + m[2] * props.header->yMin) *
	    0.001 * metrics.x_ppem / props.header->Units_Per_EM);
  if (x < xMin) {
    xMin = x;
  } else if (x > xMax) {
    xMax = x;
  }
  y = (int)((m[1] * props.header->xMax + m[3] * props.header->yMin) *
	    0.001 * metrics.x_ppem / props.header->Units_Per_EM);
  if (y < yMin) {
    yMin = y;
  } else if (y > yMax) {
    yMax = y;
  }
  x = (int)((m[0] * props.header->xMax + m[2] * props.header->yMax) *
	    0.001 * metrics.x_ppem / props.header->Units_Per_EM);
  if (x < xMin) {
    xMin = x;
  } else if (x > xMax) {
    xMax = x;
  }
  y = (int)((m[1] * props.header->xMax + m[3] * props.header->yMax) *
	    0.001 * metrics.x_ppem / props.header->Units_Per_EM);
  if (y < yMin) {
    yMin = y;
  } else if (y > yMax) {
    yMax = y;
  }
  xOffset = -xMin;
  yOffset = -yMin;
  ras.width = xMax - xMin + 1;
  ras.rows = yMax - yMin + 1;

  // set up the Raster_Map structure
  if (engine->aa) {
    ras.width = (ras.width + 3) & ~3;
    ras.cols = ras.width;
  } else {
    ras.width = (ras.width + 7) & ~7;
    ras.cols = ras.width >> 3;
  }
  ras.flow = TT_Flow_Down;
  ras.size = ras.rows * ras.cols;
  ras.bitmap = gmalloc(ras.size);

  // set up the glyph pixmap cache
  cacheAssoc = 8;
  if (ras.size <= 256) {
    cacheSets = 8;
  } else if (ras.size <= 512) {
    cacheSets = 4;
  } else if (ras.size <= 1024) {
    cacheSets = 2;
  } else {
    cacheSets = 1;
  }
  cache = (Guchar *)gmalloc(cacheSets * cacheAssoc * ras.size);
  cacheTags = (TTFontCacheTag *)gmalloc(cacheSets * cacheAssoc *
					sizeof(TTFontCacheTag));
  for (i = 0; i < cacheSets * cacheAssoc; ++i) {
    cacheTags[i].mru = i & (cacheAssoc - 1);
  }

  // create the XImage
  if (!(image = XCreateImage(engine->display, engine->visual, engine->depth,
			     ZPixmap, 0, NULL, ras.width, ras.rows, 8, 0))) {
    return;
  }
  image->data = (char *)gmalloc(ras.rows * image->bytes_per_line);

  // compute the transform matrix
  matrix.xx = (TT_Fixed)(m[0] * 65.536);
  matrix.yx = (TT_Fixed)(m[1] * 65.536);
  matrix.xy = (TT_Fixed)(m[2] * 65.536);
  matrix.yy = (TT_Fixed)(m[3] * 65.536);

  ok = gTrue;
}

TTFont::~TTFont() {
  gfree(cacheTags);
  gfree(cache);
  gfree(image->data);
  image->data = NULL;
  XDestroyImage(image);
  gfree(ras.bitmap);
  TT_Done_Glyph(glyph);
  TT_Done_Instance(instance);
}

GBool TTFont::drawChar(Drawable d, int w, int h, GC gc,
		       int x, int y, int r, int g, int b,
		       CharCode c, Unicode u) {
  TTFontEngine *engine;
  XColor xcolor;
  int bgR, bgG, bgB;
  Gulong colors[5];
  TT_Byte *p;
  TT_Byte pix;
  int xx, yy, xx1;
  int x0, y0, x1, y1, w0, h0;

  engine = fontFile->engine;

  // compute: (x0,y0) = position in destination drawable
  //          (x1,y1) = position in glyph image
  //          (w0,h0) = size of image transfer
  x0 = x - xOffset;
  y0 = y - (ras.rows - yOffset);
  x1 = 0;
  y1 = 0;
  w0 = ras.width;
  h0 = ras.rows;
  if (x0 < 0) {
    x1 = -x0;
    w0 += x0;
    x0 = 0;
  }
  if (x0 + w0 > w) {
    w0 = w - x0;
  }
  if (w0 < 0) {
    return gTrue;
  }
  if (y0 < 0) {
    y1 = -y0;
    h0 += y0;
    y0 = 0;
  }
  if (y0 + h0 > h) {
    h0 = h - y0;
  }
  if (h0 < 0) {
    return gTrue;
  }

  // read the X image
  XGetSubImage(engine->display, d, x0, y0, w0, h0, (1 << engine->depth) - 1,
	       ZPixmap, image, x1, y1);

  // generate the glyph pixmap
  if (!getGlyphPixmap(c, u)) {
    return gFalse;
  }

  if (engine->aa) {

    // compute the colors
    xcolor.pixel = XGetPixel(image, x1 + w0/2, y1 + h0/2);
    XQueryColor(engine->display, engine->colormap, &xcolor);
    bgR = xcolor.red;
    bgG = xcolor.green;
    bgB = xcolor.blue;
    colors[1] = engine->findColor((r + 3*bgR) / 4,
				  (g + 3*bgG) / 4,
				  (b + 3*bgB) / 4);
    colors[2] = engine->findColor((r + bgR) / 2,
				  (g + bgG) / 2,
				  (b + bgB) / 2);
    colors[3] = engine->findColor((3*r + bgR) / 4,
				  (3*g + bgG) / 4,
				  (3*b + bgB) / 4);
    colors[4] = engine->findColor(r, g, b);

    // stuff the glyph pixmap into the X image
    p = (TT_Byte *)ras.bitmap;
    for (yy = 0; yy < ras.rows; ++yy) {
      for (xx = 0; xx < ras.width; ++xx) {
	pix = *p++;
	if (pix > 0) {
	  if (pix > 4) {
	    pix = 4;
	  }
	  XPutPixel(image, xx, yy, colors[pix]);
	}
      }
    }

  } else {

    // one color
    colors[1] = engine->findColor(r, g, b);

    // stuff the glyph bitmap into the X image
    p = (TT_Byte *)ras.bitmap;
    for (yy = 0; yy < ras.rows; ++yy) {
      for (xx = 0; xx < ras.width; xx += 8) {
	pix = *p++;
	for (xx1 = xx; xx1 < xx + 8 && xx1 < ras.width; ++xx1) {
	  if (pix & 0x80) {
	    XPutPixel(image, xx1, yy, colors[1]);
	  }
	  pix <<= 1;
	}
      }
    }

  }

  // draw the X image
  XPutImage(engine->display, d, gc, image, x1, y1, x0, y0, w0, h0);

  return gTrue;
}

GBool TTFont::getGlyphPixmap(CharCode c, Unicode u) {
  TT_UShort idx;
  TT_Outline outline;
  int i, j, k;

  // check the cache
  i = (c & (cacheSets - 1)) * cacheAssoc;
  for (j = 0; j < cacheAssoc; ++j) {
    if ((cacheTags[i+j].mru & 0x8000) && cacheTags[i+j].code == c) {
      memcpy(ras.bitmap, cache + (i+j) * ras.size, ras.size);
      for (k = 0; k < cacheAssoc; ++k) {
	if (k != j &&
	    (cacheTags[i+k].mru & 0x7fff) < (cacheTags[i+j].mru & 0x7fff)) {
	  ++cacheTags[i+k].mru;
	}
      }
      cacheTags[i+j].mru = 0x8000;
      return gTrue;
    }
  }

  // generate the glyph pixmap or bitmap
  idx = 0; // make gcc happy
  switch (fontFile->mode) {
  case ttFontModeUnicode:
    idx = TT_Char_Index(fontFile->charMap, (TT_UShort)u);
    break;
  case ttFontModeCharCode:
    idx = TT_Char_Index(fontFile->charMap, (TT_UShort)c);
    break;
  case ttFontModeCharCodeOffset:
    idx = TT_Char_Index(fontFile->charMap,
			(TT_UShort)(c + fontFile->charMapOffset));
    break;
  case ttFontModeCodeMap:
    if (c <= 0xff) {
      idx = TT_Char_Index(fontFile->charMap,
			  (TT_UShort)(fontFile->codeMap[c] & 0xff));
    } else {
      idx = 0;
    }
    break;
  case ttFontModeCIDToGIDMap:
    if (fontFile->cidToGIDLen) {
      if ((int)c < fontFile->cidToGIDLen) {
	idx = (TT_UShort)fontFile->cidToGID[c];
      } else {
	idx = (TT_UShort)0;
      }
    } else {
      idx = (TT_UShort)c;
    }
    break;
  }
  if (TT_Load_Glyph(instance, glyph, idx, TTLOAD_DEFAULT) ||
      TT_Get_Glyph_Outline(glyph, &outline)) {
    return gFalse;
  }
  TT_Transform_Outline(&outline, &matrix);
  memset(ras.bitmap, 0, ras.size);
  if (fontFile->engine->aa) {
    if (TT_Get_Glyph_Pixmap(glyph, &ras, xOffset * 64, yOffset * 64)) {
      return gFalse;
    }
  } else {
    if (TT_Get_Glyph_Bitmap(glyph, &ras, xOffset * 64, yOffset * 64)) {
      return gFalse;
    }
  }

  // store glyph pixmap in cache
  for (j = 0; j < cacheAssoc; ++j) {
    if ((cacheTags[i+j].mru & 0x7fff) == cacheAssoc - 1) {
      cacheTags[i+j].mru = 0x8000;
      cacheTags[i+j].code = c;
      memcpy(cache + (i+j) * ras.size, ras.bitmap, ras.size);
    } else {
      ++cacheTags[i+j].mru;
    }
  }

  return gTrue;
}

#endif // !FREETYPE2 && (HAVE_FREETYPE_FREETYPE_H || HAVE_FREETYPE_H)
