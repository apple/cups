//========================================================================
//
// SFont.cc
//
//========================================================================

#ifdef __GNUC__
#pragma implementation
#endif

#include "SFont.h"

//------------------------------------------------------------------------

SFontEngine::SFontEngine(Display *display, Visual *visual, int depth,
			 Colormap colormap) {
  this->display = display;
  this->visual = visual;
  this->depth = depth;
  this->colormap = colormap;
};

SFontEngine::~SFontEngine() {
}

void SFontEngine::useTrueColor(int rMax, int rShift, int gMax, int gShift,
			       int bMax, int bShift) {
  trueColor = gTrue;
  this->rMax = rMax;
  this->rShift = rShift;
  this->gMax = gMax;
  this->gShift = gShift;
  this->bMax = bMax;
  this->bShift = bShift;
}

void SFontEngine::useColorCube(Gulong *colors, int nRGB) {
  trueColor = gFalse;
  this->colors = colors;
  this->nRGB = nRGB;
  rMax = gMax = bMax = nRGB - 1;
}

Gulong SFontEngine::findColor(int r, int g, int b) {
  int r1, g1, b1;
  Gulong pix;

  r1 = ((r & 0xffff) * rMax) / 0xffff;
  g1 = ((g & 0xffff) * rMax) / 0xffff;
  b1 = ((b & 0xffff) * rMax) / 0xffff;
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
