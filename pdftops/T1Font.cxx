//========================================================================
//
// T1Font.cc
//
// Copyright 2001-2002 Glyph & Cog, LLC
//
//========================================================================

#ifdef __GNUC__
#pragma implementation
#endif

#include <config.h>

#if HAVE_T1LIB_H

#include <math.h>
#include <string.h>
#include <X11/Xlib.h>
#include "gmem.h"
#include "GfxState.h"
#include "T1Font.h"

//------------------------------------------------------------------------

T1FontEngine::T1FontEngine(Display *displayA, Visual *visualA, int depthA,
			   Colormap colormapA, GBool aaA, GBool aaHighA):
  SFontEngine(displayA, visualA, depthA, colormapA)
{
  static unsigned long grayVals[17] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16
  };

  ok = gFalse;
  T1_SetBitmapPad(8);
  if (!T1_InitLib(NO_LOGFILE | IGNORE_CONFIGFILE | IGNORE_FONTDATABASE |
		  T1_NO_AFM)) {
    return;
  }
  aa = aaA;
  aaHigh = aaHighA;
  if (aa) {
    T1_AASetBitsPerPixel(8);
    if (aaHigh) {
      T1_AASetLevel(T1_AA_HIGH);
      T1_AAHSetGrayValues(grayVals);
    } else {
      T1_AASetLevel(T1_AA_LOW);
      T1_AASetGrayValues(0, 1, 2, 3, 4);
    }
  } else {
    T1_AANSetGrayValues(0, 1);
  }
  ok = gTrue;
}

T1FontEngine::~T1FontEngine() {
  T1_CloseLib();
}

//------------------------------------------------------------------------

T1FontFile::T1FontFile(T1FontEngine *engineA, char *fontFileName,
		       char **fontEnc, double *bboxA) {
  int encStrSize;
  char *encPtr;
  int i;

  ok = gFalse;
  engine = engineA;
  enc = NULL;
  encStr = NULL;
  for (i = 0; i < 4; ++i) {
    bbox[i] = bboxA[i];
  }

  // load the font file
  if ((id = T1_AddFont(fontFileName)) < 0) {
    return;
  }
  T1_LoadFont(id);

  // reencode it
  encStrSize = 0;
  for (i = 0; i < 256; ++i) {
    if (fontEnc[i]) {
      encStrSize += strlen(fontEnc[i]) + 1;
    }
  }
  enc = (char **)gmalloc(257 * sizeof(char *));
  encStr = (char *)gmalloc(encStrSize * sizeof(char));
  encPtr = encStr;
  for (i = 0; i < 256; ++i) {
    if (fontEnc[i]) {
      strcpy(encPtr, fontEnc[i]);
      enc[i] = encPtr;
      encPtr += strlen(encPtr) + 1;
    } else {
      enc[i] = ".notdef";
    }
  }
  enc[256] = "custom";
  T1_ReencodeFont(id, enc);

  ok = gTrue;
}

T1FontFile::~T1FontFile() {
  gfree(enc);
  gfree(encStr);
  if (id >= 0) {
    T1_DeleteFont(id);
  }
}

//------------------------------------------------------------------------

T1Font::T1Font(T1FontFile *fontFileA, double *m) {
  T1FontEngine *engine;
  T1_TMATRIX matrix;
  BBox bbox;
  double bbx0, bby0, bbx1, bby1;
  int x, y, xMin, xMax, yMin, yMax;
  int i;

  ok = gFalse;
  fontFile = fontFileA;
  engine = fontFile->engine;

  id = T1_CopyFont(fontFile->id);

  // compute font size
  size = (float)sqrt(m[2]*m[2] + m[3]*m[3]);

  // transform the four corners of the font bounding box -- the min
  // and max values form the bounding box of the transformed font
  bbx0 = fontFile->bbox[0];
  bby0 = fontFile->bbox[1];
  bbx1 = fontFile->bbox[2];
  bby1 = fontFile->bbox[3];
  // some fonts in PDF files have bboxes which are just plain wrong,
  // so we check the font file's bbox too
  bbox = T1_GetFontBBox(id);
  if (0.001 * bbox.llx < bbx0) {
    bbx0 = 0.001 * bbox.llx;
  }
  if (0.001 * bbox.lly < bby0) {
    bby0 = 0.001 * bbox.lly;
  }
  if (0.001 * bbox.urx > bbx1) {
    bbx1 = 0.001 * bbox.urx;
  }
  if (0.001 * bbox.ury > bby1) {
    bby1 = 0.001 * bbox.ury;
  }
  // some fonts are completely broken, so we fake it (with values
  // large enough that most glyphs should fit)
  if (bbx0 == 0 && bby0 == 0 && bbx1 == 0 && bby1 == 0) {
    bbx0 = bby0 = -0.5;
    bbx1 = bby1 = 1.5;
  }
  x = (int)(m[0] * bbx0 + m[2] * bby0);
  xMin = xMax = x;
  y = (int)(m[1] * bbx0 + m[3] * bby0);
  yMin = yMax = y;
  x = (int)(m[0] * bbx0 + m[2] * bby1);
  if (x < xMin) {
    xMin = x;
  } else if (x > xMax) {
    xMax = x;
  }
  y = (int)(m[1] * bbx0 + m[3] * bby1);
  if (y < yMin) {
    yMin = y;
  } else if (y > yMax) {
    yMax = y;
  }
  x = (int)(m[0] * bbx1 + m[2] * bby0);
  if (x < xMin) {
    xMin = x;
  } else if (x > xMax) {
    xMax = x;
  }
  y = (int)(m[1] * bbx1 + m[3] * bby0);
  if (y < yMin) {
    yMin = y;
  } else if (y > yMax) {
    yMax = y;
  }
  x = (int)(m[0] * bbx1 + m[2] * bby1);
  if (x < xMin) {
    xMin = x;
  } else if (x > xMax) {
    xMax = x;
  }
  y = (int)(m[1] * bbx1 + m[3] * bby1);
  if (y < yMin) {
    yMin = y;
  } else if (y > yMax) {
    yMax = y;
  }
  // This is a kludge: some buggy PDF generators embed fonts with
  // zero bounding boxes.
  if (xMax == xMin) {
    xMin = 0;
    xMax = (int)size;
  }
  if (yMax == yMin) {
    yMin = 0;
    yMax = (int)(1.2 * size);
  }
  // Another kludge: an unusually large xMin or yMin coordinate is
  // probably wrong.
  if (xMin > 0) {
    xMin = 0;
  }
  if (yMin > 0) {
    yMin = 0;
  }
  // Another kludge: t1lib doesn't correctly handle fonts with
  // real (non-integer) bounding box coordinates.
  if (xMax - xMin > 5000) {
    xMin = 0;
    xMax = (int)size;
  }
  if (yMax - yMin > 5000) {
    yMin = 0;
    yMax = (int)(1.2 * size);
  }
  // this should be (max - min + 1), but we add some padding to
  // deal with rounding errors
  glyphW = xMax - xMin + 3;
  glyphH = yMax - yMin + 3;
  if (engine->aa) {
    glyphSize = glyphW * glyphH;
  } else {
    glyphSize = ((glyphW + 7) >> 3) * glyphH;
  }

  // set up the glyph pixmap cache
  cacheAssoc = 8;
  if (glyphSize <= 256) {
    cacheSets = 8;
  } else if (glyphSize <= 512) {
    cacheSets = 4;
  } else if (glyphSize <= 1024) {
    cacheSets = 2;
  } else {
    cacheSets = 1;
  }
  cache = (Guchar *)gmalloc(cacheSets * cacheAssoc * glyphSize);
  cacheTags = (T1FontCacheTag *)gmalloc(cacheSets * cacheAssoc *
					sizeof(T1FontCacheTag));
  for (i = 0; i < cacheSets * cacheAssoc; ++i) {
    cacheTags[i].mru = i & (cacheAssoc - 1);
  }

  // create the XImage
  if (!(image = XCreateImage(engine->display, engine->visual, engine->depth,
			     ZPixmap, 0, NULL, glyphW, glyphH, 8, 0))) {
    return;
  }
  image->data = (char *)gmalloc(glyphH * image->bytes_per_line);

  // transform the font
  matrix.cxx = m[0] / size;
  matrix.cxy = m[1] / size;
  matrix.cyx = m[2] / size;
  matrix.cyy = m[3] / size;
  T1_TransformFont(id, &matrix);

  ok = gTrue;
}

T1Font::~T1Font() {
  gfree(cacheTags);
  gfree(cache);
  if (image) {
    gfree(image->data);
    image->data = NULL;
    XDestroyImage(image);
  }
  T1_DeleteFont(id);
}

GBool T1Font::drawChar(Drawable d, int w, int h, GC gc,
		       int x, int y, int r, int g, int b,
		       CharCode c, Unicode u) {
  T1FontEngine *engine;
  XColor xcolor;
  int bgR, bgG, bgB;
  Gulong colors[17];
  Guchar *p;
  int xOffset, yOffset, x0, y0, x1, y1, gw, gh, w0, h0;
  int xx, yy, xx1;
  Guchar pix, mPix;
  int i;

  engine = fontFile->engine;

  // generate the glyph pixmap
  if (!(p = getGlyphPixmap(c, &xOffset, &yOffset, &gw, &gh))) {
    return gFalse;
  }

  // compute: (x0,y0) = position in destination drawable
  //          (x1,y1) = position in glyph image
  //          (w0,h0) = size of image transfer
  x0 = x - xOffset;
  y0 = y - yOffset;
  x1 = 0;
  y1 = 0;
  w0 = gw;
  h0 = gh;
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

  if (engine->aa) {

    // compute the colors
    xcolor.pixel = XGetPixel(image, x1 + w0/2, y1 + h0/2);
    XQueryColor(engine->display, engine->colormap, &xcolor);
    bgR = xcolor.red;
    bgG = xcolor.green;
    bgB = xcolor.blue;
    if (engine->aaHigh) {
      mPix = 16;
      for (i = 1; i <= 16; ++i) {
	colors[i] = engine->findColor((i * r + (16 - i) * bgR) / 16,
				      (i * g + (16 - i) * bgG) / 16,
				      (i * b + (16 - i) * bgB) / 16);
      }
    } else {
      mPix = 4;
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
    }

    // stuff the glyph pixmap into the X image
    for (yy = 0; yy < gh; ++yy) {
      for (xx = 0; xx < gw; ++xx) {
	pix = *p++;
	if (pix > 0) {
	  if (pix > mPix) {
	    pix = mPix;
	  }
	  XPutPixel(image, xx, yy, colors[pix]);
	}
      }
    }

  } else {

    // one color
    colors[1] = engine->findColor(r, g, b);

    // stuff the glyph bitmap into the X image
    for (yy = 0; yy < gh; ++yy) {
      for (xx = 0; xx < gw; xx += 8) {
	pix = *p++;
	for (xx1 = xx; xx1 < xx + 8 && xx1 < gw; ++xx1) {
	  if (pix & 0x01) {
	    XPutPixel(image, xx1, yy, colors[1]);
	  }
	  pix >>= 1;
	}
      }
    }

  }

  // draw the X image
  XPutImage(engine->display, d, gc, image, x1, y1, x0, y0, w0, h0);

  return gTrue;
}

Guchar *T1Font::getGlyphPixmap(CharCode c, int *x, int *y, int *w, int *h) {
  T1FontEngine *engine;
  GLYPH *glyph;
  int gSize;
  int i, j, k;
  Guchar *ret;

  engine = fontFile->engine;

  // check the cache
  i = (c & (cacheSets - 1)) * cacheAssoc;
  for (j = 0; j < cacheAssoc; ++j) {
    if ((cacheTags[i+j].mru & 0x8000) && cacheTags[i+j].code == c) {
      *x = cacheTags[i+j].x;
      *y = cacheTags[i+j].y;
      *w = cacheTags[i+j].w;
      *h = cacheTags[i+j].h;
      for (k = 0; k < cacheAssoc; ++k) {
	if (k != j &&
	    (cacheTags[i+k].mru & 0x7fff) < (cacheTags[i+j].mru & 0x7fff)) {
	  ++cacheTags[i+k].mru;
	}
      }
      cacheTags[i+j].mru = 0x8000;
      return cache + (i+j) * glyphSize;
    }
  }

  // generate the glyph pixmap
  if (engine->aa) {
    glyph = T1_AASetChar(id, c, size, NULL);
  } else {
    glyph = T1_SetChar(id, c, size, NULL);
  }
  if (!glyph) {
    return NULL;
  }
  *x = -glyph->metrics.leftSideBearing;
  *y = glyph->metrics.ascent;
  *w = glyph->metrics.rightSideBearing - glyph->metrics.leftSideBearing;
  *h = glyph->metrics.ascent - glyph->metrics.descent;
  if (*w > glyphW || *h > glyphH) {
#if 1 //~ debug
    fprintf(stderr, "Weird t1lib glyph size: %d > %d or %d > %d\n",
	    *w, glyphW, *h, glyphH);
#endif
    return NULL;
  }

  // store glyph pixmap in cache
  ret = NULL;
  for (j = 0; j < cacheAssoc; ++j) {
    if ((cacheTags[i+j].mru & 0x7fff) == cacheAssoc - 1) {
      cacheTags[i+j].mru = 0x8000;
      cacheTags[i+j].code = c;
      cacheTags[i+j].x = *x;
      cacheTags[i+j].y = *y;
      cacheTags[i+j].w = *w;
      cacheTags[i+j].h = *h;
      if (engine->aa) {
	gSize = *w * *h;
      } else {
	gSize = ((*w + 7) >> 3) * *h;
      }
      ret = cache + (i+j) * glyphSize;
      if (glyph->bits) {
	memcpy(ret, glyph->bits, gSize);
      } else {
	memset(ret, 0, gSize);
      }
    } else {
      ++cacheTags[i+j].mru;
    }
  }
  return ret;
}

GBool T1Font::getCharPath(CharCode c, Unicode u, GfxState *state) {
  T1_OUTLINE *outline;
  T1_PATHSEGMENT *seg;
  T1_BEZIERSEGMENT *bez;
  double x, y, x1, y1;

  outline = T1_GetCharOutline(id, c, size, NULL);
  x = 0;
  y = 0;
  for (seg = outline; seg; seg = seg->link) {
    switch (seg->type) {
    case T1_PATHTYPE_MOVE:
      x += seg->dest.x / 65536.0;
      y += seg->dest.y / 65536.0;
      state->moveTo(x, y);
      break;
    case T1_PATHTYPE_LINE:
      x += seg->dest.x / 65536.0;
      y += seg->dest.y / 65536.0;
      state->lineTo(x, y);
      break;
    case T1_PATHTYPE_BEZIER:
      bez = (T1_BEZIERSEGMENT *)seg;
      x1 = x + bez->dest.x / 65536.0;
      y1 = y + bez->dest.y / 65536.0;
      state->curveTo(x + bez->B.x / 65536.0, y + bez->B.y / 65536.0,
		     x + bez->C.x / 65536.0, y + bez->C.y / 65536.0,
		     x1, y1);
      x = x1;
      y = y1;
      break;
    }
  }
  T1_FreeOutline(outline);
  return gTrue;
}

#endif // HAVE_T1LIB_H
