//========================================================================
//
// FTFont.cc
//
// Copyright 2001-2002 Glyph & Cog, LLC
//
//========================================================================

#ifdef __GNUC__
#pragma implementation
#endif

#include <config.h>

#if FREETYPE2 && (HAVE_FREETYPE_FREETYPE_H || HAVE_FREETYPE_H)

#include <math.h>
#include <string.h>
#include "gmem.h"
#include "freetype/ftoutln.h"
#include "freetype/internal/ftobjs.h"
#if 1 //~ cff cid->gid map
#include "freetype/internal/cfftypes.h"
#include "freetype/internal/tttypes.h"
#endif
#include "GlobalParams.h"
#include "GfxState.h"
#include "FTFont.h"

//------------------------------------------------------------------------

FTFontEngine::FTFontEngine(Display *displayA, Visual *visualA, int depthA,
			   Colormap colormapA, GBool aaA):
  SFontEngine(displayA, visualA, depthA, colormapA) {

  ok = gFalse;
  if (FT_Init_FreeType(&lib)) {
    return;
  }
  aa = aaA;
  ok = gTrue;
}

FTFontEngine::~FTFontEngine() {
  FT_Done_FreeType(lib);
}

//------------------------------------------------------------------------

FTFontFile::FTFontFile(FTFontEngine *engineA, char *fontFileName,
		       char **fontEnc, GBool pdfFontHasEncoding) {
  char *name;
  int unicodeCmap, macRomanCmap, msSymbolCmap;
  int i, j;

  ok = gFalse;
  engine = engineA;
  codeMap = NULL;
  if (FT_New_Face(engine->lib, fontFileName, 0, &face)) {
    return;
  }

  if (!strcmp(face->driver->root.clazz->module_name, "type1") ||
      !strcmp(face->driver->root.clazz->module_name, "cff")) {

    mode = ftFontModeCodeMapDirect;
    codeMap = (Guint *)gmalloc(256 * sizeof(Guint));
    for (i = 0; i < 256; ++i) {
      codeMap[i] = 0;
      if ((name = fontEnc[i])) {
	codeMap[i] = FT_Get_Name_Index(face, name);
      }
    }

  } else {

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
    for (i = 0; i < face->num_charmaps; ++i) {
      if (face->charmaps[i]->platform_id == 3 &&
	  face->charmaps[i]->encoding_id == 1) {
	unicodeCmap = i;
      } else if (face->charmaps[i]->platform_id == 1 &&
		 face->charmaps[i]->encoding_id == 0) {
	macRomanCmap = i;
      } else if (face->charmaps[i]->platform_id == 3 &&
		 face->charmaps[i]->encoding_id == 0) {
	msSymbolCmap = i;
      }
    }
    i = 0;
    mode = ftFontModeCharCode;
    charMapOffset = 0;
    if (pdfFontHasEncoding) {
      if (unicodeCmap != 0xffff) {
	i = unicodeCmap;
	mode = ftFontModeUnicode;
      } else if (macRomanCmap != 0xffff) {
	i = macRomanCmap;
	mode = ftFontModeCodeMap;
	codeMap = (Guint *)gmalloc(256 * sizeof(Guint));
	for (j = 0; j < 256; ++j) {
	  if (fontEnc[j]) {
	    codeMap[j] = globalParams->getMacRomanCharCode(fontEnc[j]);
	  } else {
	    codeMap[j] = 0;
	  }
	}
      }
    } else {
      if (macRomanCmap != 0xffff) {
	i = macRomanCmap;
	mode = ftFontModeCharCode;
      } else if (msSymbolCmap != 0xffff) {
	i = msSymbolCmap;
	mode = ftFontModeCharCodeOffset;
	charMapOffset = 0xf000;
      }
    }
    if (FT_Set_Charmap(face, face->charmaps[i])) {
      return;
    }
  }

  ok = gTrue;
}

FTFontFile::FTFontFile(FTFontEngine *engineA, char *fontFileName,
		       Gushort *cidToGIDA, int cidToGIDLenA) {
  ok = gFalse;
  engine = engineA;
  codeMap = NULL;
  if (FT_New_Face(engine->lib, fontFileName, 0, &face)) {
    return;
  }
  cidToGID = cidToGIDA;
  cidToGIDLen = cidToGIDLenA;
  mode = ftFontModeCIDToGIDMap;
  ok = gTrue;
}

FTFontFile::FTFontFile(FTFontEngine *engineA, char *fontFileName) {
  ok = gFalse;
  engine = engineA;
  codeMap = NULL;
  if (FT_New_Face(engine->lib, fontFileName, 0, &face)) {
    return;
  }
  cidToGID = NULL;
  cidToGIDLen = 0;
  mode = ftFontModeCFFCharset;
  ok = gTrue;
}

FTFontFile::~FTFontFile() {
  if (face) {
    FT_Done_Face(face);
  }
  if (codeMap) {
    gfree(codeMap);
  }
}

//------------------------------------------------------------------------

FTFont::FTFont(FTFontFile *fontFileA, double *m) {
  FTFontEngine *engine;
  FT_Face face;
  double size, div;
  int x, xMin, xMax;
  int y, yMin, yMax;
  int i;

  ok = gFalse;
  fontFile = fontFileA;
  engine = fontFile->engine;
  face = fontFile->face;
  if (FT_New_Size(face, &sizeObj)) {
    return;
  }
  face->size = sizeObj;
  size = sqrt(m[2]*m[2] + m[3]*m[3]);
  if (FT_Set_Pixel_Sizes(face, 0, (int)size)) {
    return;
  }

  div = face->bbox.xMax > 20000 ? 65536 : 1;

  // transform the four corners of the font bounding box -- the min
  // and max values form the bounding box of the transformed font
  x = (int)((m[0] * face->bbox.xMin + m[2] * face->bbox.yMin) /
	    (div * face->units_per_EM));
  xMin = xMax = x;
  y = (int)((m[1] * face->bbox.xMin + m[3] * face->bbox.yMin) /
	    (div * face->units_per_EM));
  yMin = yMax = y;
  x = (int)((m[0] * face->bbox.xMin + m[2] * face->bbox.yMax) /
	    (div * face->units_per_EM));
  if (x < xMin) {
    xMin = x;
  } else if (x > xMax) {
    xMax = x;
  }
  y = (int)((m[1] * face->bbox.xMin + m[3] * face->bbox.yMax) /
	    (div * face->units_per_EM));
  if (y < yMin) {
    yMin = y;
  } else if (y > yMax) {
    yMax = y;
  }
  x = (int)((m[0] * face->bbox.xMax + m[2] * face->bbox.yMin) /
	    (div * face->units_per_EM));
  if (x < xMin) {
    xMin = x;
  } else if (x > xMax) {
    xMax = x;
  }
  y = (int)((m[1] * face->bbox.xMax + m[3] * face->bbox.yMin) /
	    (div * face->units_per_EM));
  if (y < yMin) {
    yMin = y;
  } else if (y > yMax) {
    yMax = y;
  }
  x = (int)((m[0] * face->bbox.xMax + m[2] * face->bbox.yMax) /
	    (div * face->units_per_EM));
  if (x < xMin) {
    xMin = x;
  } else if (x > xMax) {
    xMax = x;
  }
  y = (int)((m[1] * face->bbox.xMax + m[3] * face->bbox.yMax) /
	    (div * face->units_per_EM));
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
  // this should be (max - min + 1), but we add some padding to
  // deal with rounding errors
  glyphW = xMax - xMin + 3;
  glyphH = yMax - yMin + 3;
  // another kludge: some CJK TT fonts have bogus bboxes, so add more
  // padding
  if (face->num_glyphs > 1000) {
    glyphW += glyphW >> 1;
    glyphH += glyphH >> 1;
  }
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
  cacheTags = (FTFontCacheTag *)gmalloc(cacheSets * cacheAssoc *
					sizeof(FTFontCacheTag));
  for (i = 0; i < cacheSets * cacheAssoc; ++i) {
    cacheTags[i].mru = i & (cacheAssoc - 1);
  }

  // create the XImage
  if (!(image = XCreateImage(engine->display, engine->visual, engine->depth,
			     ZPixmap, 0, NULL, glyphW, glyphH, 8, 0))) {
    return;
  }
  image->data = (char *)gmalloc(glyphH * image->bytes_per_line);

  // compute the transform matrix
  matrix.xx = (FT_Fixed)((m[0] / size) * 65536);
  matrix.yx = (FT_Fixed)((m[1] / size) * 65536);
  matrix.xy = (FT_Fixed)((m[2] / size) * 65536);
  matrix.yy = (FT_Fixed)((m[3] / size) * 65536);

  ok = gTrue;
}

FTFont::~FTFont() {
  gfree(cacheTags);
  gfree(cache);
  gfree(image->data);
  image->data = NULL;
  XDestroyImage(image);
}

GBool FTFont::drawChar(Drawable d, int w, int h, GC gc,
		       int x, int y, int r, int g, int b,
		       CharCode c, Unicode u) {
  FTFontEngine *engine;
  XColor xcolor;
  int bgR, bgG, bgB;
  Gulong colors[5];
  Guchar *p;
  int pix;
  int xOffset, yOffset, x0, y0, x1, y1, gw, gh, w0, h0;
  int xx, yy, xx1;

  engine = fontFile->engine;

  // no Unicode index for this char - don't draw anything
  if (fontFile->mode == ftFontModeUnicode && u == 0) {
    return gFalse;
  }

  // generate the glyph pixmap
  if (!(p = getGlyphPixmap(c, u, &xOffset, &yOffset, &gw, &gh))) {
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
    for (yy = 0; yy < gh; ++yy) {
      for (xx = 0; xx < gw; ++xx) {
	pix = *p++ & 0xff;
	// this is a heuristic which seems to produce decent
	// results -- the linear mapping would be:
	// pix = (pix * 5) / 256;
	pix = ((pix + 10) * 5) / 256;
	if (pix > 4) {
	  pix = 4;
	}
	if (pix > 0) {
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

Guchar *FTFont::getGlyphPixmap(CharCode c, Unicode u,
			       int *x, int *y, int *w, int *h) {
  FT_GlyphSlot slot;
  FT_UInt idx;
  int gSize;
  int i, j, k;
  Guchar *ret;

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

  // generate the glyph pixmap or bitmap
  fontFile->face->size = sizeObj;
  FT_Set_Transform(fontFile->face, &matrix, NULL);
  slot = fontFile->face->glyph;
  idx = getGlyphIndex(c, u);
  // if we have the FT2 bytecode interpreter, autohinting won't be used
#ifdef TT_CONFIG_OPTION_BYTECODE_INTERPRETER
  if (FT_Load_Glyph(fontFile->face, idx, FT_LOAD_DEFAULT)) {
    return gFalse;
  }
#else
  // FT2's autohinting doesn't always work very well (especially with
  // font subsets), so turn it off if anti-aliasing is enabled; if
  // anti-aliasing is disabled, this seems to be a tossup - some fonts
  // look better with hinting, some without, so leave hinting on
  if (FT_Load_Glyph(fontFile->face, idx,
		    fontFile->engine->aa ? FT_LOAD_NO_HINTING
		                         : FT_LOAD_DEFAULT)) {
    return gFalse;
  }
#endif
  if (FT_Render_Glyph(slot,
		      fontFile->engine->aa ? ft_render_mode_normal :
		                             ft_render_mode_mono)) {
    return gFalse;
  }
  *x = -slot->bitmap_left;
  *y = slot->bitmap_top;
  *w = slot->bitmap.width;
  *h = slot->bitmap.rows;
  if (*w > glyphW || *h > glyphH) {
#if 1 //~ debug
    fprintf(stderr, "Weird FreeType glyph size: %d > %d or %d > %d\n",
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
      if (fontFile->engine->aa) {
	gSize = *w * *h;
      } else {
	gSize = ((*w + 7) >> 3) * *h;
      }
      ret = cache + (i+j) * glyphSize;
      memcpy(ret, slot->bitmap.buffer, gSize);
    } else {
      ++cacheTags[i+j].mru;
    }
  }
  return ret;
}

GBool FTFont::getCharPath(CharCode c, Unicode u, GfxState *state) {
  static FT_Outline_Funcs outlineFuncs = {
    &charPathMoveTo,
    &charPathLineTo,
    &charPathConicTo,
    &charPathCubicTo,
    0, 0
  };
  FT_GlyphSlot slot;
  FT_UInt idx;
  FT_Glyph glyph;

  fontFile->face->size = sizeObj;
  FT_Set_Transform(fontFile->face, &matrix, NULL);
  slot = fontFile->face->glyph;
  idx = getGlyphIndex(c, u);
#ifdef TT_CONFIG_OPTION_BYTECODE_INTERPRETER
  if (FT_Load_Glyph(fontFile->face, idx, FT_LOAD_DEFAULT)) {
    return gFalse;
  }
#else
  // FT2's autohinting doesn't always work very well (especially with
  // font subsets), so turn it off if anti-aliasing is enabled; if
  // anti-aliasing is disabled, this seems to be a tossup - some fonts
  // look better with hinting, some without, so leave hinting on
  if (FT_Load_Glyph(fontFile->face, idx,
		    fontFile->engine->aa ? FT_LOAD_NO_HINTING
		                         : FT_LOAD_DEFAULT)) {
    return gFalse;
  }
#endif
  if (FT_Get_Glyph(slot, &glyph)) {
    return gFalse;
  }
  FT_Outline_Decompose(&((FT_OutlineGlyph)glyph)->outline,
		       &outlineFuncs, state);
  return gTrue;
}

int FTFont::charPathMoveTo(FT_Vector *pt, void *state) {
  ((GfxState *)state)->moveTo(pt->x / 64.0, -pt->y / 64.0);
  return 0;
}

int FTFont::charPathLineTo(FT_Vector *pt, void *state) {
  ((GfxState *)state)->lineTo(pt->x / 64.0, -pt->y / 64.0);
  return 0;
}

int FTFont::charPathConicTo(FT_Vector *ctrl, FT_Vector *pt, void *state) {
  double x0, y0, x1, y1, x2, y2, x3, y3, xc, yc;

  x0 = ((GfxState *)state)->getCurX();
  y0 = ((GfxState *)state)->getCurY();
  xc = ctrl->x / 64.0;
  yc = -ctrl->y / 64.0;
  x3 = pt->x / 64.0;
  y3 = -pt->y / 64.0;

  // A second-order Bezier curve is defined by two endpoints, p0 and
  // p3, and one control point, pc:
  //
  //     p(t) = (1-t)^2*p0 + t*(1-t)*pc + t^2*p3
  //
  // A third-order Bezier curve is defined by the same two endpoints,
  // p0 and p3, and two control points, p1 and p2:
  //
  //     p(t) = (1-t)^3*p0 + 3t*(1-t)^2*p1 + 3t^2*(1-t)*p2 + t^3*p3
  //
  // Applying some algebra, we can convert a second-order curve to a
  // third-order curve:
  //
  //     p1 = (1/3) * (p0 + 2pc)
  //     p2 = (1/3) * (2pc + p3)

  x1 = (1.0 / 3.0) * (x0 + 2 * xc);
  y1 = (1.0 / 3.0) * (y0 + 2 * yc);
  x2 = (1.0 / 3.0) * (2 * xc + x3);
  y2 = (1.0 / 3.0) * (2 * yc + y3);

  ((GfxState *)state)->curveTo(x1, y1, x2, y2, x3, y3);
  return 0;
}

int FTFont::charPathCubicTo(FT_Vector *ctrl1, FT_Vector *ctrl2,
			    FT_Vector *pt, void *state) {
  ((GfxState *)state)->curveTo(ctrl1->x / 64.0, -ctrl1->y / 64.0,
			       ctrl2->x / 64.0, -ctrl2->y / 64.0,
			       pt->x / 64.0, -pt->y / 64.0);
  return 0;
}

FT_UInt FTFont::getGlyphIndex(CharCode c, Unicode u) {
  FT_UInt idx;
  int j;

  idx = 0; // make gcc happy
  switch (fontFile->mode) {
  case ftFontModeUnicode:
    idx = FT_Get_Char_Index(fontFile->face, (FT_ULong)u);
    break;
  case ftFontModeCharCode:
    idx = FT_Get_Char_Index(fontFile->face, (FT_ULong)c);
    break;
  case ftFontModeCharCodeOffset:
    idx = FT_Get_Char_Index(fontFile->face,
			    (FT_ULong)(c + fontFile->charMapOffset));
    break;
  case ftFontModeCodeMap:
    if (c <= 0xff) {
      idx = FT_Get_Char_Index(fontFile->face, (FT_ULong)fontFile->codeMap[c]);
    } else {
      idx = 0;
    }
    break;
  case ftFontModeCodeMapDirect:
    if (c <= 0xff) {
      idx = (FT_UInt)fontFile->codeMap[c];
    } else {
      idx = 0;
    }
    break;
  case ftFontModeCIDToGIDMap:
    if (fontFile->cidToGIDLen) {
      if ((int)c < fontFile->cidToGIDLen) {
	idx = (FT_UInt)fontFile->cidToGID[c];
      } else {
	idx = (FT_UInt)0;
      }
    } else {
      idx = (FT_UInt)c;
    }
    break;
  case ftFontModeCFFCharset:
#if 1 //~ cff cid->gid map
#if FREETYPE_MAJOR == 2 && FREETYPE_MINOR == 0
    CFF_Font *cff = (CFF_Font *)((TT_Face)fontFile->face)->extra.data;
#else
    CFF_Font cff = (CFF_Font)((TT_Face)fontFile->face)->extra.data;
#endif
    idx = 0;
    for (j = 0; j < (int)cff->num_glyphs; ++j) {
      if (cff->charset.sids[j] == c) {
	idx = j;
	break;
      }
    }
#endif
    break;
  }
  return idx;
}

#endif // FREETYPE2 && (HAVE_FREETYPE_FREETYPE_H || HAVE_FREETYPE_H)
