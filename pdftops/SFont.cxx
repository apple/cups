//========================================================================
//
// SFont.cc
//
// Copyright 2001-2002 Glyph & Cog, LLC
//
//========================================================================

#ifdef __GNUC__
#pragma implementation
#endif

#include <config.h>
#include "SFont.h"

//------------------------------------------------------------------------

SFontEngine::SFontEngine(Display *displayA, Visual *visualA, int depthA,
			 Colormap colormapA) {
  display = displayA;
  visual = visualA;
  depth = depthA;
  colormap = colormapA;
}

SFontEngine::~SFontEngine() {
}

void SFontEngine::useTrueColor(int rMaxA, int rShiftA, int gMaxA, int gShiftA,
			       int bMaxA, int bShiftA) {
  trueColor = gTrue;
  rMax = rMaxA;
  rShift = rShiftA;
  gMax = gMaxA;
  gShift = gShiftA;
  bMax = bMaxA;
  bShift = bShiftA;
}

void SFontEngine::useColorCube(Gulong *colorsA, int nRGBA) {
  trueColor = gFalse;
  colors = colorsA;
  nRGB = nRGBA;
  rMax = gMax = bMax = nRGB - 1;
}

Gulong SFontEngine::findColor(int r, int g, int b) {
  int r1, g1, b1;
  Gulong pix;

  r1 = ((r & 0xffff) * rMax) / 0xffff;
  g1 = ((g & 0xffff) * gMax) / 0xffff;
  b1 = ((b & 0xffff) * bMax) / 0xffff;
  if (trueColor) {
    pix = (r1 << rShift) + (g1 << gShift) + (b1 << bShift);
  } else {
    pix = colors[(r1 * nRGB + g1) * nRGB + b1];
  }
  return pix;
}

//------------------------------------------------------------------------

SFontFile::SFontFile() {
}

SFontFile::~SFontFile() {
}

//------------------------------------------------------------------------

SFont::SFont() {
}

SFont::~SFont() {
}

GBool SFont::getCharPath(CharCode c, Unicode u, GfxState *state) {
  return gFalse;
}
