//========================================================================
//
// OutputDev.cc
//
// Copyright 1996 Derek B. Noonburg
//
//========================================================================

#ifdef __GNUC__
#pragma implementation
#endif

#include <stddef.h>
#include "Object.h"
#include "Stream.h"
#include "GfxState.h"
#include "OutputDev.h"

//------------------------------------------------------------------------
// OutputDev
//------------------------------------------------------------------------

void OutputDev::setDefaultCTM(double *ctm1) {
  int i;
  double det;

  for (i = 0; i < 6; ++i)
    ctm[i] = ctm1[i];
  det = 1 / (ctm[0] * ctm[3] - ctm[1] * ctm[2]);
  ictm[0] = ctm[3] * det;
  ictm[1] = -ctm[1] * det;
  ictm[2] = -ctm[2] * det;
  ictm[3] = ctm[0] * det;
  ictm[4] = (ctm[2] * ctm[5] - ctm[3] * ctm[4]) * det;
  ictm[5] = (ctm[1] * ctm[4] - ctm[0] * ctm[5]) * det;
}

void OutputDev::cvtDevToUser(int dx, int dy, double *ux, double *uy) {
  *ux = ictm[0] * dx + ictm[2] * dy + ictm[4];
  *uy = ictm[1] * dx + ictm[3] * dy + ictm[5];
}

void OutputDev::cvtUserToDev(double ux, double uy, int *dx, int *dy) {
  *dx = (int)(ctm[0] * ux + ctm[2] * uy + ctm[4] + 0.5);
  *dy = (int)(ctm[1] * ux + ctm[3] * uy + ctm[5] + 0.5);
}

void OutputDev::updateAll(GfxState *state) {
  updateLineDash(state);
  updateFlatness(state);
  updateLineJoin(state);
  updateLineCap(state);
  updateMiterLimit(state);
  updateLineWidth(state);
  updateFillColor(state);
  updateStrokeColor(state);
  updateFont(state);
}

void OutputDev::drawImageMask(GfxState *state, Stream *str,
			      int width, int height, GBool invert,
			      GBool inlineImg) {
  int i, j;

  if (inlineImg) {
    str->reset();
    j = height * ((width + 7) / 8);
    for (i = 0; i < j; ++i)
      str->getChar();
  }
}

void OutputDev::drawImage(GfxState *state, Stream *str, int width,
			  int height, GfxImageColorMap *colorMap,
			  GBool inlineImg) {
  int i, j;

  if (inlineImg) {
    str->reset();
    j = height * ((width * colorMap->getNumPixelComps() *
		   colorMap->getBits() + 7) / 8);
    for (i = 0; i < j; ++i)
      str->getChar();
  }
}

#if OPI_SUPPORT
void OutputDev::opiBegin(GfxState *state, Dict *opiDict) {
}

void OutputDev::opiEnd(GfxState *state, Dict *opiDict) {
}
#endif
