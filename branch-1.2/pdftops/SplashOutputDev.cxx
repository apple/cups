//========================================================================
//
// SplashOutputDev.cc
//
// Copyright 2003 Glyph & Cog, LLC
//
//========================================================================

#include <config.h>

#ifdef USE_GCC_PRAGMAS
#pragma implementation
#endif

#include <string.h>
#include <math.h>
#include "gfile.h"
#include "GlobalParams.h"
#include "Error.h"
#include "Object.h"
#include "GfxState.h"
#include "GfxFont.h"
#include "Link.h"
#include "CharCodeToUnicode.h"
#include "FontEncodingTables.h"
#include "FoFiTrueType.h"
#include "SplashBitmap.h"
#include "SplashGlyphBitmap.h"
#include "SplashPattern.h"
#include "SplashScreen.h"
#include "SplashPath.h"
#include "SplashState.h"
#include "SplashErrorCodes.h"
#include "SplashFontEngine.h"
#include "SplashFont.h"
#include "SplashFontFile.h"
#include "SplashFontFileID.h"
#include "Splash.h"
#include "SplashOutputDev.h"

//------------------------------------------------------------------------
// Font substitutions
//------------------------------------------------------------------------

struct SplashOutFontSubst {
  char *name;
  double mWidth;
};

// index: {symbolic:12, fixed:8, serif:4, sans-serif:0} + bold*2 + italic
static SplashOutFontSubst splashOutSubstFonts[16] = {
  {"Helvetica",             0.833},
  {"Helvetica-Oblique",     0.833},
  {"Helvetica-Bold",        0.889},
  {"Helvetica-BoldOblique", 0.889},
  {"Times-Roman",           0.788},
  {"Times-Italic",          0.722},
  {"Times-Bold",            0.833},
  {"Times-BoldItalic",      0.778},
  {"Courier",               0.600},
  {"Courier-Oblique",       0.600},
  {"Courier-Bold",          0.600},
  {"Courier-BoldOblique",   0.600},
  {"Symbol",                0.576},
  {"Symbol",                0.576},
  {"Symbol",                0.576},
  {"Symbol",                0.576}
};

//------------------------------------------------------------------------

#define soutRound(x) ((int)(x + 0.5))

//------------------------------------------------------------------------
// SplashOutFontFileID
//------------------------------------------------------------------------

class SplashOutFontFileID: public SplashFontFileID {
public:

  SplashOutFontFileID(Ref *rA) { r = *rA; substIdx = -1; }

  ~SplashOutFontFileID() {}

  GBool matches(SplashFontFileID *id) {
    return ((SplashOutFontFileID *)id)->r.num == r.num &&
           ((SplashOutFontFileID *)id)->r.gen == r.gen;
  }

  void setSubstIdx(int substIdxA) { substIdx = substIdxA; }
  int getSubstIdx() { return substIdx; }

private:

  Ref r;
  int substIdx;
};

//------------------------------------------------------------------------
// T3FontCache
//------------------------------------------------------------------------

struct T3FontCacheTag {
  Gushort code;
  Gushort mru;			// valid bit (0x8000) and MRU index
};

class T3FontCache {
public:

  T3FontCache(Ref *fontID, double m11A, double m12A,
	      double m21A, double m22A,
	      int glyphXA, int glyphYA, int glyphWA, int glyphHA,
	      GBool aa);
  ~T3FontCache();
  GBool matches(Ref *idA, double m11A, double m12A,
		double m21A, double m22A)
    { return fontID.num == idA->num && fontID.gen == idA->gen &&
	     m11 == m11A && m12 == m12A && m21 == m21A && m22 == m22A; }

  Ref fontID;			// PDF font ID
  double m11, m12, m21, m22;	// transform matrix
  int glyphX, glyphY;		// pixel offset of glyph bitmaps
  int glyphW, glyphH;		// size of glyph bitmaps, in pixels
  int glyphSize;		// size of glyph bitmaps, in bytes
  int cacheSets;		// number of sets in cache
  int cacheAssoc;		// cache associativity (glyphs per set)
  Guchar *cacheData;		// glyph pixmap cache
  T3FontCacheTag *cacheTags;	// cache tags, i.e., char codes
};

T3FontCache::T3FontCache(Ref *fontIDA, double m11A, double m12A,
			 double m21A, double m22A,
			 int glyphXA, int glyphYA, int glyphWA, int glyphHA,
			 GBool aa) {
  int i;

  fontID = *fontIDA;
  m11 = m11A;
  m12 = m12A;
  m21 = m21A;
  m22 = m22A;
  glyphX = glyphXA;
  glyphY = glyphYA;
  glyphW = glyphWA;
  glyphH = glyphHA;
  if (aa) {
    glyphSize = glyphW * glyphH;
  } else {
    glyphSize = ((glyphW + 7) >> 3) * glyphH;
  }
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
  cacheData = (Guchar *)gmalloc(cacheSets * cacheAssoc * glyphSize);
  cacheTags = (T3FontCacheTag *)gmalloc(cacheSets * cacheAssoc *
					sizeof(T3FontCacheTag));
  for (i = 0; i < cacheSets * cacheAssoc; ++i) {
    cacheTags[i].mru = i & (cacheAssoc - 1);
  }
}

T3FontCache::~T3FontCache() {
  gfree(cacheData);
  gfree(cacheTags);
}

struct T3GlyphStack {
  Gushort code;			// character code
  double x, y;			// position to draw the glyph

  //----- cache info
  T3FontCache *cache;		// font cache for the current font
  T3FontCacheTag *cacheTag;	// pointer to cache tag for the glyph
  Guchar *cacheData;		// pointer to cache data for the glyph

  //----- saved state
  SplashBitmap *origBitmap;
  Splash *origSplash;
  double origCTM4, origCTM5;

  T3GlyphStack *next;		// next object on stack
};

//------------------------------------------------------------------------
// SplashOutputDev
//------------------------------------------------------------------------

SplashOutputDev::SplashOutputDev(SplashColorMode colorModeA,
				 int bitmapRowPadA,
				 GBool reverseVideoA,
				 SplashColor paperColorA) {
  colorMode = colorModeA;
  bitmapRowPad = bitmapRowPadA;
  reverseVideo = reverseVideoA;
  paperColor = paperColorA;

  xref = NULL;

  bitmap = new SplashBitmap(1, 1, bitmapRowPad, colorMode);
  splash = new Splash(bitmap);
  splash->clear(paperColor);

  fontEngine = NULL;

  nT3Fonts = 0;
  t3GlyphStack = NULL;

  font = NULL;
  needFontUpdate = gFalse;
  textClipPath = NULL;

  underlayCbk = NULL;
  underlayCbkData = NULL;
}

SplashOutputDev::~SplashOutputDev() {
  int i;

  for (i = 0; i < nT3Fonts; ++i) {
    delete t3FontCache[i];
  }
  if (fontEngine) {
    delete fontEngine;
  }
  if (splash) {
    delete splash;
  }
  if (bitmap) {
    delete bitmap;
  }
}

void SplashOutputDev::startDoc(XRef *xrefA) {
  int i;

  xref = xrefA;
  if (fontEngine) {
    delete fontEngine;
  }
  fontEngine = new SplashFontEngine(
#if HAVE_T1LIB_H
				    globalParams->getEnableT1lib(),
#endif
#if HAVE_FREETYPE_FREETYPE_H || HAVE_FREETYPE_H
				    globalParams->getEnableFreeType(),
#endif
				    globalParams->getAntialias());
  for (i = 0; i < nT3Fonts; ++i) {
    delete t3FontCache[i];
  }
  nT3Fonts = 0;
}

void SplashOutputDev::startPage(int pageNum, GfxState *state) {
  int w, h;
  SplashColor color;

  w = state ? (int)(state->getPageWidth() + 0.5) : 1;
  h = state ? (int)(state->getPageHeight() + 0.5) : 1;
  if (splash) {
    delete splash;
  }
  if (!bitmap || w != bitmap->getWidth() || h != bitmap->getHeight()) {
    if (bitmap) {
      delete bitmap;
    }
    bitmap = new SplashBitmap(w, h, bitmapRowPad, colorMode);
  }
  splash = new Splash(bitmap);
  switch (colorMode) {
  case splashModeMono1: color.mono1 = 0; break;
  case splashModeMono8: color.mono8 = 0; break;
  case splashModeRGB8: color.rgb8 = splashMakeRGB8(0, 0, 0); break;
  case splashModeBGR8Packed: color.bgr8 = splashMakeBGR8(0, 0, 0); break;
  }
  splash->setStrokePattern(new SplashSolidColor(color));
  splash->setFillPattern(new SplashSolidColor(color));
  splash->setLineCap(splashLineCapButt);
  splash->setLineJoin(splashLineJoinMiter);
  splash->setLineDash(NULL, 0, 0);
  splash->setMiterLimit(10);
  splash->setFlatness(1);
  splash->clear(paperColor);

  if (underlayCbk) {
    (*underlayCbk)(underlayCbkData);
  }
}

void SplashOutputDev::endPage() {
}

void SplashOutputDev::drawLink(Link *link, Catalog *catalog) {
  double x1, y1, x2, y2;
  LinkBorderStyle *borderStyle;
  GfxRGB rgb;
  double gray;
  double *dash;
  int dashLength;
  SplashCoord dashList[20];
  SplashPath *path;
  int x, y, i;

  link->getRect(&x1, &y1, &x2, &y2);
  borderStyle = link->getBorderStyle();
  if (borderStyle->getWidth() > 0) {
    borderStyle->getColor(&rgb.r, &rgb.g, &rgb.b);
    gray = 0.299 * rgb.r + 0.587 * rgb.g + 0.114 * rgb.b;
    if (gray > 1) {
      gray = 1;
    }
    splash->setStrokePattern(getColor(gray, &rgb));
    splash->setLineWidth((SplashCoord)borderStyle->getWidth());
    borderStyle->getDash(&dash, &dashLength);
    if (borderStyle->getType() == linkBorderDashed && dashLength > 0) {
      if (dashLength > 20) {
	dashLength = 20;
      }
      for (i = 0; i < dashLength; ++i) {
	dashList[i] = (SplashCoord)dash[i];
      }
      splash->setLineDash(dashList, dashLength, 0);
    }
    path = new SplashPath();
    if (borderStyle->getType() == linkBorderUnderlined) {
      cvtUserToDev(x1, y1, &x, &y);
      path->moveTo((SplashCoord)x, (SplashCoord)y);
      cvtUserToDev(x2, y1, &x, &y);
      path->lineTo((SplashCoord)x, (SplashCoord)y);
    } else {
      cvtUserToDev(x1, y1, &x, &y);
      path->moveTo((SplashCoord)x, (SplashCoord)y);
      cvtUserToDev(x2, y1, &x, &y);
      path->lineTo((SplashCoord)x, (SplashCoord)y);
      cvtUserToDev(x2, y2, &x, &y);
      path->lineTo((SplashCoord)x, (SplashCoord)y);
      cvtUserToDev(x1, y2, &x, &y);
      path->lineTo((SplashCoord)x, (SplashCoord)y);
      path->close();
    }
    splash->stroke(path);
    delete path;
  }
}

void SplashOutputDev::saveState(GfxState *state) {
  splash->saveState();
}

void SplashOutputDev::restoreState(GfxState *state) {
  splash->restoreState();
  needFontUpdate = gTrue;
}

void SplashOutputDev::updateAll(GfxState *state) {
  updateLineDash(state);
  updateLineJoin(state);
  updateLineCap(state);
  updateLineWidth(state);
  updateFlatness(state);
  updateMiterLimit(state);
  updateFillColor(state);
  updateStrokeColor(state);
  needFontUpdate = gTrue;
}

void SplashOutputDev::updateCTM(GfxState *state, double m11, double m12,
				double m21, double m22,
				double m31, double m32) {
  updateLineDash(state);
  updateLineJoin(state);
  updateLineCap(state);
  updateLineWidth(state);
}

void SplashOutputDev::updateLineDash(GfxState *state) {
  double *dashPattern;
  int dashLength;
  double dashStart;
  SplashCoord dash[20];
  SplashCoord phase;
  int i;

  state->getLineDash(&dashPattern, &dashLength, &dashStart);
  if (dashLength > 20) {
    dashLength = 20;
  }
  for (i = 0; i < dashLength; ++i) {
    dash[i] =  (SplashCoord)state->transformWidth(dashPattern[i]);
    if (dash[i] < 1) {
      dash[i] = 1;
    }
  }
  phase = (SplashCoord)state->transformWidth(dashStart);
  splash->setLineDash(dash, dashLength, phase);
}

void SplashOutputDev::updateFlatness(GfxState *state) {
  splash->setFlatness(state->getFlatness());
}

void SplashOutputDev::updateLineJoin(GfxState *state) {
  splash->setLineJoin(state->getLineJoin());
}

void SplashOutputDev::updateLineCap(GfxState *state) {
  splash->setLineCap(state->getLineCap());
}

void SplashOutputDev::updateMiterLimit(GfxState *state) {
  splash->setMiterLimit(state->getMiterLimit());
}

void SplashOutputDev::updateLineWidth(GfxState *state) {
  splash->setLineWidth(state->getTransformedLineWidth());
}

void SplashOutputDev::updateFillColor(GfxState *state) {
  double gray;
  GfxRGB rgb;

  state->getFillGray(&gray);
  state->getFillRGB(&rgb);
  splash->setFillPattern(getColor(gray, &rgb));
}

void SplashOutputDev::updateStrokeColor(GfxState *state) {
  double gray;
  GfxRGB rgb;

  state->getStrokeGray(&gray);
  state->getStrokeRGB(&rgb);
  splash->setStrokePattern(getColor(gray, &rgb));
}

SplashPattern *SplashOutputDev::getColor(double gray, GfxRGB *rgb) {
  SplashPattern *pattern;
  SplashColor color0, color1;
  double r, g, b;

  if (reverseVideo) {
    gray = 1 - gray;
    r = 1 - rgb->r;
    g = 1 - rgb->g;
    b = 1 - rgb->b;
  } else {
    r = rgb->r;
    g = rgb->g;
    b = rgb->b;
  }

  pattern = NULL; // make gcc happy
  switch (colorMode) {
  case splashModeMono1:
    color0.mono1 = 0;
    color1.mono1 = 1;
    pattern = new SplashHalftone(color0, color1,
				 splash->getScreen()->copy(),
				 (SplashCoord)gray);
    break;
  case splashModeMono8:
    color1.mono8 = soutRound(255 * gray);
    pattern = new SplashSolidColor(color1);
    break;
  case splashModeRGB8:
    color1.rgb8 = splashMakeRGB8(soutRound(255 * r),
				 soutRound(255 * g),
				 soutRound(255 * b));
    pattern = new SplashSolidColor(color1);
    break;
  case splashModeBGR8Packed:
    color1.bgr8 = splashMakeBGR8(soutRound(255 * r),
				 soutRound(255 * g),
				 soutRound(255 * b));
    pattern = new SplashSolidColor(color1);
    break;
  }

  return pattern;
}

void SplashOutputDev::updateFont(GfxState *state) {
  GfxFont *gfxFont;
  GfxFontType fontType;
  SplashOutFontFileID *id;
  SplashFontFile *fontFile;
  FoFiTrueType *ff;
  Ref embRef;
  Object refObj, strObj;
  GString *tmpFileName, *fileName, *substName;
  FILE *tmpFile;
  Gushort *codeToGID;
  DisplayFontParam *dfp;
  CharCodeToUnicode *ctu;
  double m11, m12, m21, m22, w1, w2;
  SplashCoord mat[4];
  char *name;
  Unicode uBuf[8];
  int c, substIdx, n, code, cmap;

  needFontUpdate = gFalse;
  font = NULL;
  tmpFileName = NULL;
  substIdx = -1;
  dfp = NULL;

  if (!(gfxFont = state->getFont())) {
    goto err1;
  }
  fontType = gfxFont->getType();
  if (fontType == fontType3) {
    goto err1;
  }

  // check the font file cache
  id = new SplashOutFontFileID(gfxFont->getID());
  if ((fontFile = fontEngine->getFontFile(id))) {
    delete id;

  } else {

    // if there is an embedded font, write it to disk
    if (gfxFont->getEmbeddedFontID(&embRef)) {
      if (!openTempFile(&tmpFileName, &tmpFile, "wb", NULL)) {
	error(-1, "Couldn't create temporary font file");
	goto err2;
      }
      refObj.initRef(embRef.num, embRef.gen);
      refObj.fetch(xref, &strObj);
      refObj.free();
      strObj.streamReset();
      while ((c = strObj.streamGetChar()) != EOF) {
	fputc(c, tmpFile);
      }
      strObj.streamClose();
      strObj.free();
      fclose(tmpFile);
      fileName = tmpFileName;

    // if there is an external font file, use it
    } else if (!(fileName = gfxFont->getExtFontFile())) {

      // look for a display font mapping or a substitute font
      if (gfxFont->isCIDFont()) {
	if (((GfxCIDFont *)gfxFont)->getCollection()) {
	  dfp = globalParams->
	          getDisplayCIDFont(gfxFont->getName(),
				    ((GfxCIDFont *)gfxFont)->getCollection());
	}
      } else {
	if (gfxFont->getName()) {
	  dfp = globalParams->getDisplayFont(gfxFont->getName());
	}
	if (!dfp) {
	  // 8-bit font substitution
	  if (gfxFont->isFixedWidth()) {
	    substIdx = 8;
	  } else if (gfxFont->isSerif()) {
	    substIdx = 4;
	  } else {
	    substIdx = 0;
	  }
	  if (gfxFont->isBold()) {
	    substIdx += 2;
	  }
	  if (gfxFont->isItalic()) {
	    substIdx += 1;
	  }
	  substName = new GString(splashOutSubstFonts[substIdx].name);
	  dfp = globalParams->getDisplayFont(substName);
	  delete substName;
	  id->setSubstIdx(substIdx);
	}
      }
      if (!dfp) {
	error(-1, "Couldn't find a font for '%s'",
	      gfxFont->getName() ? gfxFont->getName()->getCString()
	                         : "(unnamed)");
	goto err2;
      }
      switch (dfp->kind) {
      case displayFontT1:
	fileName = dfp->t1.fileName;
	fontType = gfxFont->isCIDFont() ? fontCIDType0 : fontType1;
	break;
      case displayFontTT:
	fileName = dfp->tt.fileName;
	fontType = gfxFont->isCIDFont() ? fontCIDType2 : fontTrueType;
	break;
      }
    }

    // load the font file
    switch (fontType) {
    case fontType1:
      if (!(fontFile = fontEngine->loadType1Font(
			   id,
			   fileName->getCString(),
			   fileName == tmpFileName,
			   ((Gfx8BitFont *)gfxFont)->getEncoding()))) {
	error(-1, "Couldn't create a font for '%s'",
	      gfxFont->getName() ? gfxFont->getName()->getCString()
	                         : "(unnamed)");
	goto err2;
      }
      break;
    case fontType1C:
      if (!(fontFile = fontEngine->loadType1CFont(
			   id,
			   fileName->getCString(),
			   fileName == tmpFileName,
			   ((Gfx8BitFont *)gfxFont)->getEncoding()))) {
	error(-1, "Couldn't create a font for '%s'",
	      gfxFont->getName() ? gfxFont->getName()->getCString()
	                         : "(unnamed)");
	goto err2;
      }
      break;
    case fontTrueType:
      if (!(ff = FoFiTrueType::load(fileName->getCString()))) {
	goto err2;
      }
      codeToGID = ((Gfx8BitFont *)gfxFont)->getCodeToGIDMap(ff);
      delete ff;
      if (!(fontFile = fontEngine->loadTrueTypeFont(
			   id,
			   fileName->getCString(),
			   fileName == tmpFileName,
			   codeToGID, 256))) {
	error(-1, "Couldn't create a font for '%s'",
	      gfxFont->getName() ? gfxFont->getName()->getCString()
	                         : "(unnamed)");
	goto err2;
      }
      break;
    case fontCIDType0:
    case fontCIDType0C:
      if (!(fontFile = fontEngine->loadCIDFont(
			   id,
			   fileName->getCString(),
			   fileName == tmpFileName))) {
	error(-1, "Couldn't create a font for '%s'",
	      gfxFont->getName() ? gfxFont->getName()->getCString()
	                         : "(unnamed)");
	goto err2;
      }
      break;
    case fontCIDType2:
      codeToGID = NULL;
      n = 0;
      if (dfp) {
	// create a CID-to-GID mapping, via Unicode
	if ((ctu = ((GfxCIDFont *)gfxFont)->getToUnicode())) {
	  if ((ff = FoFiTrueType::load(fileName->getCString()))) {
	    // look for a Unicode cmap
	    for (cmap = 0; cmap < ff->getNumCmaps(); ++cmap) {
	      if ((ff->getCmapPlatform(cmap) == 3 &&
		   ff->getCmapEncoding(cmap) == 1) ||
		  ff->getCmapPlatform(cmap) == 0) {
		break;
	      }
	    }
	    if (cmap < ff->getNumCmaps()) {
	      // map CID -> Unicode -> GID
	      n = ctu->getLength();
	      codeToGID = (Gushort *)gmalloc(n * sizeof(Gushort));
	      for (code = 0; code < n; ++code) {
		if (ctu->mapToUnicode(code, uBuf, 8) > 0) {
		  codeToGID[code] = ff->mapCodeToGID(cmap, uBuf[0]);
		} else {
		  codeToGID[code] = 0;
		}
	      }
	    }
	    delete ff;
	  }
	  ctu->decRefCnt();
	} else {
	  error(-1, "Couldn't find a mapping to Unicode for font '%s'",
		gfxFont->getName() ? gfxFont->getName()->getCString()
		                   : "(unnamed)");
	}
      } else {
	if (((GfxCIDFont *)gfxFont)->getCIDToGID()) {
	  n = ((GfxCIDFont *)gfxFont)->getCIDToGIDLen();
	  codeToGID = (Gushort *)gmalloc(n * sizeof(Gushort));
	  memcpy(codeToGID, ((GfxCIDFont *)gfxFont)->getCIDToGID(),
		 n * sizeof(Gushort));
	}
      }
      if (!(fontFile = fontEngine->loadTrueTypeFont(
			   id,
			   fileName->getCString(),
			   fileName == tmpFileName,
			   codeToGID, n))) {
	error(-1, "Couldn't create a font for '%s'",
	      gfxFont->getName() ? gfxFont->getName()->getCString()
	                         : "(unnamed)");
	goto err2;
      }
      break;
    default:
      // this shouldn't happen
      goto err2;
    }
  }

  // get the font matrix
  state->getFontTransMat(&m11, &m12, &m21, &m22);
  m11 *= state->getHorizScaling();
  m12 *= state->getHorizScaling();

  // for substituted fonts: adjust the font matrix -- compare the
  // width of 'm' in the original font and the substituted font
  substIdx = ((SplashOutFontFileID *)fontFile->getID())->getSubstIdx();
  if (substIdx >= 0) {
    for (code = 0; code < 256; ++code) {
      if ((name = ((Gfx8BitFont *)gfxFont)->getCharName(code)) &&
	  name[0] == 'm' && name[1] == '\0') {
	break;
      }
    }
    if (code < 256) {
      w1 = ((Gfx8BitFont *)gfxFont)->getWidth(code);
      w2 = splashOutSubstFonts[substIdx].mWidth;
      if (!gfxFont->isSymbolic()) {
	// if real font is substantially narrower than substituted
	// font, reduce the font size accordingly
	if (w1 > 0.01 && w1 < 0.9 * w2) {
	  w1 /= w2;
	  m11 *= w1;
	  m21 *= w1;
	}
      }
    }
  }

  // create the scaled font
  mat[0] = m11;  mat[1] = -m12;
  mat[2] = m21;  mat[3] = -m22;
  if (fabs(mat[0] * mat[3] - mat[1] * mat[2]) < 0.01) {
    // avoid a singular (or close-to-singular) matrix
    mat[0] = 0.01;  mat[1] = 0;
    mat[2] = 0;     mat[3] = 0.01;
  }
  font = fontEngine->getFont(fontFile, mat);

  if (tmpFileName) {
    delete tmpFileName;
  }
  return;

 err2:
  delete id;
 err1:
  if (tmpFileName) {
    delete tmpFileName;
  }
  return;
}

void SplashOutputDev::stroke(GfxState *state) {
  SplashPath *path;

  path = convertPath(state, state->getPath());
  splash->stroke(path);
  delete path;
}

void SplashOutputDev::fill(GfxState *state) {
  SplashPath *path;

  path = convertPath(state, state->getPath());
  splash->fill(path, gFalse);
  delete path;
}

void SplashOutputDev::eoFill(GfxState *state) {
  SplashPath *path;

  path = convertPath(state, state->getPath());
  splash->fill(path, gTrue);
  delete path;
}

void SplashOutputDev::clip(GfxState *state) {
  SplashPath *path;

  path = convertPath(state, state->getPath());
  splash->clipToPath(path, gFalse);
  delete path;
}

void SplashOutputDev::eoClip(GfxState *state) {
  SplashPath *path;

  path = convertPath(state, state->getPath());
  splash->clipToPath(path, gTrue);
  delete path;
}

SplashPath *SplashOutputDev::convertPath(GfxState *state, GfxPath *path) {
  SplashPath *sPath;
  GfxSubpath *subpath;
  double x1, y1, x2, y2, x3, y3;
  int i, j;

  sPath = new SplashPath();
  for (i = 0; i < path->getNumSubpaths(); ++i) {
    subpath = path->getSubpath(i);
    if (subpath->getNumPoints() > 0) {
      state->transform(subpath->getX(0), subpath->getY(0), &x1, &y1);
      sPath->moveTo((SplashCoord)x1, (SplashCoord)y1);
      j = 1;
      while (j < subpath->getNumPoints()) {
	if (subpath->getCurve(j)) {
	  state->transform(subpath->getX(j), subpath->getY(j), &x1, &y1);
	  state->transform(subpath->getX(j+1), subpath->getY(j+1), &x2, &y2);
	  state->transform(subpath->getX(j+2), subpath->getY(j+2), &x3, &y3);
	  sPath->curveTo((SplashCoord)x1, (SplashCoord)y1,
			 (SplashCoord)x2, (SplashCoord)y2,
			 (SplashCoord)x3, (SplashCoord)y3);
	  j += 3;
	} else {
	  state->transform(subpath->getX(j), subpath->getY(j), &x1, &y1);
	  sPath->lineTo((SplashCoord)x1, (SplashCoord)y1);
	  ++j;
	}
      }
      if (subpath->isClosed()) {
	sPath->close();
      }
    }
  }
  return sPath;
}

void SplashOutputDev::drawChar(GfxState *state, double x, double y,
			       double dx, double dy,
			       double originX, double originY,
			       CharCode code, Unicode *u, int uLen) {
  double x1, y1;
  SplashPath *path;
  int render;

  if (needFontUpdate) {
    updateFont(state);
  }
  if (!font) {
    return;
  }

  // check for invisible text -- this is used by Acrobat Capture
  render = state->getRender();
  if (render == 3) {
    return;
  }

  x -= originX;
  y -= originY;
  state->transform(x, y, &x1, &y1);

  // fill
  if (!(render & 1)) {
    splash->fillChar((SplashCoord)x1, (SplashCoord)y1, code, font);
  }

  // stroke
  if ((render & 3) == 1 || (render & 3) == 2) {
    if ((path = font->getGlyphPath(code))) {
      path->offset((SplashCoord)x1, (SplashCoord)y1);
      splash->stroke(path);
      delete path;
    }
  }

  // clip
  if (render & 4) {
    path = font->getGlyphPath(code);
    path->offset((SplashCoord)x1, (SplashCoord)y1);
    if (textClipPath) {
      textClipPath->append(path);
      delete path;
    } else {
      textClipPath = path;
    }
  }
}

GBool SplashOutputDev::beginType3Char(GfxState *state, double x, double y,
				      double dx, double dy,
				      CharCode code, Unicode *u, int uLen) {
  GfxFont *gfxFont;
  Ref *fontID;
  double *ctm, *bbox;
  T3FontCache *t3Font;
  T3GlyphStack *t3gs;
  double x1, y1, xMin, yMin, xMax, yMax, xt, yt;
  int i, j;

  if (!(gfxFont = state->getFont())) {
    return gFalse;
  }
  fontID = gfxFont->getID();
  ctm = state->getCTM();
  state->transform(0, 0, &xt, &yt);

  // is it the first (MRU) font in the cache?
  if (!(nT3Fonts > 0 &&
	t3FontCache[0]->matches(fontID, ctm[0], ctm[1], ctm[2], ctm[3]))) {

    // is the font elsewhere in the cache?
    for (i = 1; i < nT3Fonts; ++i) {
      if (t3FontCache[i]->matches(fontID, ctm[0], ctm[1], ctm[2], ctm[3])) {
	t3Font = t3FontCache[i];
	for (j = i; j > 0; --j) {
	  t3FontCache[j] = t3FontCache[j - 1];
	}
	t3FontCache[0] = t3Font;
	break;
      }
    }
    if (i >= nT3Fonts) {

      // create new entry in the font cache
      if (nT3Fonts == splashOutT3FontCacheSize) {
	delete t3FontCache[nT3Fonts - 1];
	--nT3Fonts;
      }
      for (j = nT3Fonts; j > 0; --j) {
	t3FontCache[j] = t3FontCache[j - 1];
      }
      ++nT3Fonts;
      bbox = gfxFont->getFontBBox();
      if (bbox[0] == 0 && bbox[1] == 0 && bbox[2] == 0 && bbox[3] == 0) {
	// broken bounding box -- just take a guess
	xMin = xt - 5;
	xMax = xMin + 30;
	yMax = yt + 15;
	yMin = yMax - 45;
      } else {
	state->transform(bbox[0], bbox[1], &x1, &y1);
	xMin = xMax = x1;
	yMin = yMax = y1;
	state->transform(bbox[0], bbox[3], &x1, &y1);
	if (x1 < xMin) {
	  xMin = x1;
	} else if (x1 > xMax) {
	  xMax = x1;
	}
	if (y1 < yMin) {
	  yMin = y1;
	} else if (y1 > yMax) {
	  yMax = y1;
	}
	state->transform(bbox[2], bbox[1], &x1, &y1);
	if (x1 < xMin) {
	  xMin = x1;
	} else if (x1 > xMax) {
	  xMax = x1;
	}
	if (y1 < yMin) {
	  yMin = y1;
	} else if (y1 > yMax) {
	  yMax = y1;
	}
	state->transform(bbox[2], bbox[3], &x1, &y1);
	if (x1 < xMin) {
	  xMin = x1;
	} else if (x1 > xMax) {
	  xMax = x1;
	}
	if (y1 < yMin) {
	  yMin = y1;
	} else if (y1 > yMax) {
	  yMax = y1;
	}
      }
      t3FontCache[0] = new T3FontCache(fontID, ctm[0], ctm[1], ctm[2], ctm[3],
	                               (int)floor(xMin - xt),
				       (int)floor(yMin - yt),
				       (int)ceil(xMax) - (int)floor(xMin) + 3,
				       (int)ceil(yMax) - (int)floor(yMin) + 3,
				       colorMode != splashModeMono1);
    }
  }
  t3Font = t3FontCache[0];

  // is the glyph in the cache?
  i = (code & (t3Font->cacheSets - 1)) * t3Font->cacheAssoc;
  for (j = 0; j < t3Font->cacheAssoc; ++j) {
    if ((t3Font->cacheTags[i+j].mru & 0x8000) &&
	t3Font->cacheTags[i+j].code == code) {
      drawType3Glyph(t3Font, &t3Font->cacheTags[i+j],
		     t3Font->cacheData + (i+j) * t3Font->glyphSize,
		     xt, yt);
      return gTrue;
    }
  }

  // push a new Type 3 glyph record
  t3gs = new T3GlyphStack();
  t3gs->next = t3GlyphStack;
  t3GlyphStack = t3gs;
  t3GlyphStack->code = code;
  t3GlyphStack->x = xt;
  t3GlyphStack->y = yt;
  t3GlyphStack->cache = t3Font;
  t3GlyphStack->cacheTag = NULL;
  t3GlyphStack->cacheData = NULL;

  return gFalse;
}

void SplashOutputDev::endType3Char(GfxState *state) {
  T3GlyphStack *t3gs;
  double *ctm;

  if (t3GlyphStack->cacheTag) {
    memcpy(t3GlyphStack->cacheData, bitmap->getDataPtr().mono8,
	   t3GlyphStack->cache->glyphSize);
    delete bitmap;
    delete splash;
    bitmap = t3GlyphStack->origBitmap;
    splash = t3GlyphStack->origSplash;
    ctm = state->getCTM();
    state->setCTM(ctm[0], ctm[1], ctm[2], ctm[3],
		  t3GlyphStack->origCTM4, t3GlyphStack->origCTM5);
    drawType3Glyph(t3GlyphStack->cache,
		   t3GlyphStack->cacheTag, t3GlyphStack->cacheData,
		   t3GlyphStack->x, t3GlyphStack->y);
  }
  t3gs = t3GlyphStack;
  t3GlyphStack = t3gs->next;
  delete t3gs;
}

void SplashOutputDev::type3D0(GfxState *state, double wx, double wy) {
}

void SplashOutputDev::type3D1(GfxState *state, double wx, double wy,
			      double llx, double lly, double urx, double ury) {
  double *ctm;
  T3FontCache *t3Font;
  SplashColor color;
  double xt, yt, xMin, xMax, yMin, yMax, x1, y1;
  int i, j;

  t3Font = t3GlyphStack->cache;

  // check for a valid bbox
  state->transform(0, 0, &xt, &yt);
  state->transform(llx, lly, &x1, &y1);
  xMin = xMax = x1;
  yMin = yMax = y1;
  state->transform(llx, ury, &x1, &y1);
  if (x1 < xMin) {
    xMin = x1;
  } else if (x1 > xMax) {
    xMax = x1;
  }
  if (y1 < yMin) {
    yMin = y1;
  } else if (y1 > yMax) {
    yMax = y1;
  }
  state->transform(urx, lly, &x1, &y1);
  if (x1 < xMin) {
    xMin = x1;
  } else if (x1 > xMax) {
    xMax = x1;
  }
  if (y1 < yMin) {
    yMin = y1;
  } else if (y1 > yMax) {
    yMax = y1;
  }
  state->transform(urx, ury, &x1, &y1);
  if (x1 < xMin) {
    xMin = x1;
  } else if (x1 > xMax) {
    xMax = x1;
  }
  if (y1 < yMin) {
    yMin = y1;
  } else if (y1 > yMax) {
    yMax = y1;
  }
  if (xMin - xt < t3Font->glyphX ||
      yMin - yt < t3Font->glyphY ||
      xMax - xt > t3Font->glyphX + t3Font->glyphW ||
      yMax - yt > t3Font->glyphY + t3Font->glyphH) {
    error(-1, "Bad bounding box in Type 3 glyph");
    return;
  }

  // allocate a cache entry
  i = (t3GlyphStack->code & (t3Font->cacheSets - 1)) * t3Font->cacheAssoc;
  for (j = 0; j < t3Font->cacheAssoc; ++j) {
    if ((t3Font->cacheTags[i+j].mru & 0x7fff) == t3Font->cacheAssoc - 1) {
      t3Font->cacheTags[i+j].mru = 0x8000;
      t3Font->cacheTags[i+j].code = t3GlyphStack->code;
      t3GlyphStack->cacheTag = &t3Font->cacheTags[i+j];
      t3GlyphStack->cacheData = t3Font->cacheData + (i+j) * t3Font->glyphSize;
    } else {
      ++t3Font->cacheTags[i+j].mru;
    }
  }

  // save state
  t3GlyphStack->origBitmap = bitmap;
  t3GlyphStack->origSplash = splash;
  ctm = state->getCTM();
  t3GlyphStack->origCTM4 = ctm[4];
  t3GlyphStack->origCTM5 = ctm[5];

  // create the temporary bitmap
  if (colorMode == splashModeMono1) {
    bitmap = new SplashBitmap(t3Font->glyphW, t3Font->glyphH, 1,
			      splashModeMono1);
    splash = new Splash(bitmap);
    color.mono1 = 0;
    splash->clear(color);
    color.mono1 = 1;
  } else {
    bitmap = new SplashBitmap(t3Font->glyphW, t3Font->glyphH, 1,
			      splashModeMono8);
    splash = new Splash(bitmap);
    color.mono8 = 0x00;
    splash->clear(color);
    color.mono8 = 0xff;
  }
  splash->setFillPattern(new SplashSolidColor(color));
  splash->setStrokePattern(new SplashSolidColor(color));
  //~ this should copy other state from t3GlyphStack->origSplash?
  state->setCTM(ctm[0], ctm[1], ctm[2], ctm[3],
		-t3Font->glyphX, -t3Font->glyphY);
}

void SplashOutputDev::drawType3Glyph(T3FontCache *t3Font,
				     T3FontCacheTag *tag, Guchar *data,
				     double x, double y) {
  SplashGlyphBitmap glyph;

  glyph.x = -t3Font->glyphX;
  glyph.y = -t3Font->glyphY;
  glyph.w = t3Font->glyphW;
  glyph.h = t3Font->glyphH;
  glyph.aa = colorMode != splashModeMono1;
  glyph.data = data;
  glyph.freeData = gFalse;
  splash->fillGlyph((SplashCoord)x, (SplashCoord)y, &glyph);
}

void SplashOutputDev::endTextObject(GfxState *state) {
  if (textClipPath) {
    splash->clipToPath(textClipPath, gFalse);
    delete textClipPath;
    textClipPath = NULL;
  }
}

struct SplashOutImageMaskData {
  ImageStream *imgStr;
  int nPixels, idx;
  GBool invert;
};

GBool SplashOutputDev::imageMaskSrc(void *data, SplashMono1 *pixel) {
  SplashOutImageMaskData *imgMaskData = (SplashOutImageMaskData *)data;
  Guchar pix;

  if (imgMaskData->idx >= imgMaskData->nPixels) {
    return gFalse;
  }
  //~ use getLine
  imgMaskData->imgStr->getPixel(&pix);
  if (!imgMaskData->invert) {
    pix ^= 1;
  }
  *pixel = pix;
  ++imgMaskData->idx;
  return gTrue;
}

void SplashOutputDev::drawImageMask(GfxState *state, Object *ref, Stream *str,
				    int width, int height, GBool invert,
				    GBool inlineImg) {
  double *ctm;
  SplashCoord mat[6];
  SplashOutImageMaskData imgMaskData;
  Guchar pix;

  ctm = state->getCTM();
  mat[0] = ctm[0];
  mat[1] = ctm[1];
  mat[2] = -ctm[2];
  mat[3] = -ctm[3];
  mat[4] = ctm[2] + ctm[4];
  mat[5] = ctm[3] + ctm[5];

  imgMaskData.imgStr = new ImageStream(str, width, 1, 1);
  imgMaskData.imgStr->reset();
  imgMaskData.nPixels = width * height;
  imgMaskData.idx = 0;
  imgMaskData.invert = invert;

  splash->fillImageMask(&imageMaskSrc, &imgMaskData, width, height, mat);
  if (inlineImg) {
    while (imageMaskSrc(&imgMaskData, &pix)) ;
  }

  delete imgMaskData.imgStr;
}

struct SplashOutImageData {
  ImageStream *imgStr;
  GfxImageColorMap *colorMap;
  int *maskColors;
  SplashOutputDev *out;
  int nPixels, idx;
};

GBool SplashOutputDev::imageSrc(void *data, SplashColor *pixel,
				Guchar *alpha) {
  SplashOutImageData *imgData = (SplashOutImageData *)data;
  Guchar pix[gfxColorMaxComps];
  GfxRGB rgb;
  double gray;
  int i;

  if (imgData->idx >= imgData->nPixels) {
    return gFalse;
  }

  //~ use getLine
  imgData->imgStr->getPixel(pix);
  switch (imgData->out->colorMode) {
  case splashModeMono1:
  case splashModeMono8:
    imgData->colorMap->getGray(pix, &gray);
    pixel->mono8 = soutRound(255 * gray);
    break;
  case splashModeRGB8:
    imgData->colorMap->getRGB(pix, &rgb);
    pixel->rgb8 = splashMakeRGB8(soutRound(255 * rgb.r),
				 soutRound(255 * rgb.g),
				 soutRound(255 * rgb.b));
    break;
  case splashModeBGR8Packed:
    imgData->colorMap->getRGB(pix, &rgb);
    pixel->bgr8 = splashMakeBGR8(soutRound(255 * rgb.r),
				 soutRound(255 * rgb.g),
				 soutRound(255 * rgb.b));
    break;
  }

  if (imgData->maskColors) {
    *alpha = 0;
    for (i = 0; i < imgData->colorMap->getNumPixelComps(); ++i) {
      if (pix[i] < imgData->maskColors[2*i] ||
	  pix[i] > imgData->maskColors[2*i+1]) {
	*alpha = 1;
	break;
      }
    }
  } else {
    *alpha = 1;
  }

  ++imgData->idx;
  return gTrue;
}

void SplashOutputDev::drawImage(GfxState *state, Object *ref, Stream *str,
				int width, int height,
				GfxImageColorMap *colorMap,
				int *maskColors, GBool inlineImg) {
  double *ctm;
  SplashCoord mat[6];
  SplashOutImageData imgData;
  SplashColor pix;
  Guchar alpha;

  ctm = state->getCTM();
  mat[0] = ctm[0];
  mat[1] = ctm[1];
  mat[2] = -ctm[2];
  mat[3] = -ctm[3];
  mat[4] = ctm[2] + ctm[4];
  mat[5] = ctm[3] + ctm[5];

  imgData.imgStr = new ImageStream(str, width,
				   colorMap->getNumPixelComps(),
				   colorMap->getBits());
  imgData.imgStr->reset();
  imgData.colorMap = colorMap;
  imgData.maskColors = maskColors;
  imgData.out = this;
  imgData.nPixels = width * height;
  imgData.idx = 0;

  splash->drawImage(&imageSrc, &imgData,
		    (colorMode == splashModeMono1) ? splashModeMono8
			                           : colorMode,
		    width, height, mat);
  if (inlineImg) {
    while (imageSrc(&imgData, &pix, &alpha)) ;
  }

  delete imgData.imgStr;
}

int SplashOutputDev::getBitmapWidth() {
  return bitmap->getWidth();
}

int SplashOutputDev::getBitmapHeight() {
  return bitmap->getHeight();
}

void SplashOutputDev::xorRectangle(int x0, int y0, int x1, int y1,
				   SplashPattern *pattern) {
  SplashPath *path;

  path = new SplashPath();
  path->moveTo((SplashCoord)x0, (SplashCoord)y0);
  path->lineTo((SplashCoord)x1, (SplashCoord)y0);
  path->lineTo((SplashCoord)x1, (SplashCoord)y1);
  path->lineTo((SplashCoord)x0, (SplashCoord)y1);
  path->close();
  splash->setFillPattern(pattern);
  splash->xorFill(path, gTrue);
  delete path;
}

void SplashOutputDev::setFillColor(int r, int g, int b) {
  GfxRGB rgb;
  double gray;

  rgb.r = r / 255.0;
  rgb.g = g / 255.0;
  rgb.b = b / 255.0;
  gray = 0.299 * rgb.r + 0.587 * rgb.g + 0.114 * rgb.g;
  splash->setFillPattern(getColor(gray, &rgb));
}

SplashFont *SplashOutputDev::getFont(GString *name, double *mat) {
  DisplayFontParam *dfp;
  Ref ref;
  SplashOutFontFileID *id;
  SplashFontFile *fontFile;
  SplashFont *fontObj;
  int i;

  for (i = 0; i < 16; ++i) {
    if (!name->cmp(splashOutSubstFonts[i].name)) {
      break;
    }
  }
  if (i == 16) {
    return NULL;
  }
  ref.num = i;
  ref.gen = -1;
  id = new SplashOutFontFileID(&ref);

  // check the font file cache
  if ((fontFile = fontEngine->getFontFile(id))) {
    delete id;

  // load the font file
  } else {
    dfp = globalParams->getDisplayFont(name);
    if (dfp->kind != displayFontT1) {
      return NULL;
    }
    fontFile = fontEngine->loadType1Font(id, dfp->t1.fileName->getCString(),
					 gFalse, winAnsiEncoding);
  }

  // create the scaled font
  fontObj = fontEngine->getFont(fontFile, (SplashCoord *)mat);

  return fontObj;
}
