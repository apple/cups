//========================================================================
//
// TTFont.cc
//
//========================================================================

#ifdef __GNUC__
#pragma implementation
#endif

#if HAVE_FREETYPE_FREETYPE_H

#include <string.h>
#include "gmem.h"
#include "TTFont.h"

//------------------------------------------------------------------------

TTFontEngine::TTFontEngine(Display *display, Visual *visual, int depth,
			   Colormap colormap, GBool aa):
  SFontEngine(display, visual, depth, colormap) {
  static TT_Byte ttPalette[5] = {0, 1, 2, 3, 4};

  ok = gFalse;
  if (TT_Init_FreeType(&engine)) {
    return;
  }
  this->aa = aa;
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

TTFontFile::TTFontFile(TTFontEngine *engine, char *fontFileName) {
  TT_Face_Properties props;
  TT_UShort platform, encoding, i;

  ok = gFalse;
  this->engine = engine;
  if (TT_Open_Face(engine->engine, fontFileName, &face)) {
    return;
  }
  if (TT_Get_Face_Properties(face, &props)) {
    return;
  }

  // Choose a cmap:
  // 1. If the font contains a Windows-symbol cmap, use it.
  // 2. Otherwise, use the first cmap in the TTF file.
  // 3. If the Windows-Symbol cmap is used (from either step 1 or step
  //    2), offset all character indexes by 0xf000.
  // This seems to match what acroread does, but may need further
  // tweaking.
  for (i = 0; i < props.num_CharMaps; ++i) {
    if (!TT_Get_CharMap_ID(face, i, &platform, &encoding)) {
      if (platform == 3 && encoding == 0) {
	break;
      }
    }
  }
  if (i >= props.num_CharMaps) {
    i = 0;
    TT_Get_CharMap_ID(face, i, &platform, &encoding);
  }
  if (platform == 3 && encoding == 0) {
    charMapOffset = 0xf000;
  } else {
    charMapOffset = 0;
  }
  TT_Get_CharMap(face, i, &charMap);

  ok = gTrue;
}

TTFontFile::~TTFontFile() {
  TT_Close_Face(face);
}

//------------------------------------------------------------------------

TTFont::TTFont(TTFontFile *fontFile, double *m) {
  TTFontEngine *engine;
  TT_Face_Properties props;
  TT_Instance_Metrics metrics;
  int x, xMin, xMax;
  int y, yMin, yMax;
  int i;

  ok = gFalse;
  this->fontFile = fontFile;
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
		       int x, int y, int r, int g, int b, Gushort c) {
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
  if (!getGlyphPixmap(c)) {
    return gFalse;
  }

  if (engine->aa) {

    // compute the colors
    xcolor.pixel = XGetPixel(image, x1, y1);
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

GBool TTFont::getGlyphPixmap(Gushort c) {
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
  idx = TT_Char_Index(fontFile->charMap, fontFile->charMapOffset + c);
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

#endif // HAVE_FREETYPE_FREETYPE_H
