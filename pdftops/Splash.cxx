//========================================================================
//
// Splash.cc
//
//========================================================================

#include <config.h>

#ifdef USE_GCC_PRAGMAS
#pragma implementation
#endif

#include <stdlib.h>
#include <string.h>
#include "gmem.h"
#include "SplashErrorCodes.h"
#include "SplashMath.h"
#include "SplashBitmap.h"
#include "SplashState.h"
#include "SplashPath.h"
#include "SplashXPath.h"
#include "SplashXPathScanner.h"
#include "SplashPattern.h"
#include "SplashScreen.h"
#include "SplashClip.h"
#include "SplashFont.h"
#include "SplashGlyphBitmap.h"
#include "Splash.h"

//------------------------------------------------------------------------
// Splash
//------------------------------------------------------------------------

Splash::Splash(SplashBitmap *bitmapA) {
  bitmap = bitmapA;
  state = new SplashState(bitmap->width, bitmap->height);
  debugMode = gFalse;
}

Splash::~Splash() {
  while (state->next) {
    restoreState();
  }
  delete state;
}

//------------------------------------------------------------------------
// state read
//------------------------------------------------------------------------


SplashPattern *Splash::getStrokePattern() {
  return state->strokePattern;
}

SplashPattern *Splash::getFillPattern() {
  return state->fillPattern;
}

SplashScreen *Splash::getScreen() {
  return state->screen;
}

SplashCoord Splash::getLineWidth() {
  return state->lineWidth;
}

int Splash::getLineCap() {
  return state->lineCap;
}

int Splash::getLineJoin() {
  return state->lineJoin;
}

SplashCoord Splash::getMiterLimit() {
  return state->miterLimit;
}

SplashCoord Splash::getFlatness() {
  return state->flatness;
}

SplashCoord *Splash::getLineDash() {
  return state->lineDash;
}

int Splash::getLineDashLength() {
  return state->lineDashLength;
}

SplashCoord Splash::getLineDashPhase() {
  return state->lineDashPhase;
}

SplashClip *Splash::getClip() {
  return state->clip;
}

//------------------------------------------------------------------------
// state write
//------------------------------------------------------------------------

void Splash::setStrokePattern(SplashPattern *strokePattern) {
  state->setStrokePattern(strokePattern);
}

void Splash::setFillPattern(SplashPattern *fillPattern) {
  state->setFillPattern(fillPattern);
}

void Splash::setScreen(SplashScreen *screen) {
  state->setScreen(screen);
}

void Splash::setLineWidth(SplashCoord lineWidth) {
  state->lineWidth = lineWidth;
}

void Splash::setLineCap(int lineCap) {
  state->lineCap = lineCap;
}

void Splash::setLineJoin(int lineJoin) {
  state->lineJoin = lineJoin;
}

void Splash::setMiterLimit(SplashCoord miterLimit) {
  state->miterLimit = miterLimit;
}

void Splash::setFlatness(SplashCoord flatness) {
  if (flatness < 1) {
    state->flatness = 1;
  } else {
    state->flatness = flatness;
  }
}

void Splash::setLineDash(SplashCoord *lineDash, int lineDashLength,
			 SplashCoord lineDashPhase) {
  state->setLineDash(lineDash, lineDashLength, lineDashPhase);
}

void Splash::clipResetToRect(SplashCoord x0, SplashCoord y0,
			     SplashCoord x1, SplashCoord y1) {
  state->clip->resetToRect(x0, y0, x1, y1);
}

SplashError Splash::clipToRect(SplashCoord x0, SplashCoord y0,
			       SplashCoord x1, SplashCoord y1) {
  return state->clip->clipToRect(x0, y0, x1, y1);
}

SplashError Splash::clipToPath(SplashPath *path, GBool eo) {
  return state->clip->clipToPath(path, state->flatness, eo);
}

//------------------------------------------------------------------------
// state save/restore
//------------------------------------------------------------------------

void Splash::saveState() {
  SplashState *newState;

  newState = state->copy();
  newState->next = state;
  state = newState;
}

SplashError Splash::restoreState() {
  SplashState *oldState;

  if (!state->next) {
    return splashErrNoSave;
  }
  oldState = state;
  state = state->next;
  delete oldState;
  return splashOk;
}

//------------------------------------------------------------------------
// drawing operations
//------------------------------------------------------------------------

void Splash::clear(SplashColor color) {
  SplashMono1P *mono1;
  SplashMono8 *mono8;
  SplashRGB8 *rgb8Row, *rgb8;
  SplashBGR8P *bgr8Row, *bgr8;
  SplashMono1 data;
  int n, i, x, y;

  switch (bitmap->mode) {
  case splashModeMono1:
    n = bitmap->rowSize * bitmap->height;
    data = color.mono1 ? 0xff : 0x00;
    for (i = 0, mono1 = bitmap->data.mono1; i < n; ++i, ++mono1) {
      *mono1 = data;
    }
    break;
  case splashModeMono8:
    n = bitmap->rowSize * bitmap->height;
    for (i = 0, mono8 = bitmap->data.mono8; i < n; ++i, ++mono8) {
      *mono8 = color.mono8;
    }
    break;
  case splashModeRGB8:
    rgb8Row = bitmap->data.rgb8;
    for (y = 0; y < bitmap->height; ++y) {
      rgb8 = rgb8Row;
      for (x = 0; x < bitmap->width; ++x) {
	*rgb8++ = color.rgb8;
      }
      rgb8Row += bitmap->rowSize >> 2;
    }
    break;
  case splashModeBGR8Packed:
    bgr8Row = bitmap->data.bgr8;
    for (y = 0; y < bitmap->height; ++y) {
      bgr8 = bgr8Row;
      for (x = 0; x < bitmap->width; ++x) {
	bgr8[2] = splashBGR8R(color.bgr8);
	bgr8[1] = splashBGR8G(color.bgr8);
	bgr8[0] = splashBGR8B(color.bgr8);
	bgr8 += 3;
      }
      bgr8Row += bitmap->rowSize;
    }
    break;
  }
}

SplashError Splash::stroke(SplashPath *path) {
  SplashXPath *xPath, *xPath2;

  if (debugMode) {
    printf("stroke [dash:%d] [width:%.2f]:\n",
	   state->lineDashLength, state->lineWidth);
    dumpPath(path);
  }
  if (path->length == 0) {
    return splashErrEmptyPath;
  }
  xPath = new SplashXPath(path, state->flatness, gFalse);
  if (state->lineDashLength > 0) {
    xPath2 = makeDashedPath(xPath);
    delete xPath;
    xPath = xPath2;
  }
  if (state->lineWidth <= 1) {
    strokeNarrow(xPath);
  } else {
    strokeWide(xPath);
  }
  delete xPath;
  return splashOk;
}

void Splash::strokeNarrow(SplashXPath *xPath) {
  SplashXPathSeg *seg;
  int x0, x1, x2, x3, y0, y1, x, y, t;
  SplashCoord dx, dy, dxdy;
  SplashClipResult clipRes;
  int i;

  for (i = 0, seg = xPath->segs; i < xPath->length; ++i, ++seg) {

    x0 = splashFloor(seg->x0);
    x1 = splashFloor(seg->x1);
    y0 = splashFloor(seg->y0);
    y1 = splashFloor(seg->y1);

    // horizontal segment
    if (y0 == y1) {
      if (x0 > x1) {
	t = x0; x0 = x1; x1 = t;
      }
      if ((clipRes = state->clip->testSpan(x0, x1, y0))
	  != splashClipAllOutside) {
	drawSpan(x0, x1, y0, state->strokePattern,
		 clipRes == splashClipAllInside);
      }

    // segment with |dx| > |dy|
    } else if (splashAbs(seg->dxdy) > 1) {
      dx = seg->x1 - seg->x0;
      dy = seg->y1 - seg->y0;
      dxdy = seg->dxdy;
      if (y0 > y1) {
	t = y0; y0 = y1; y1 = t;
	t = x0; x0 = x1; x1 = t;
	dx = -dx;
	dy = -dy;
      }
      if ((clipRes = state->clip->testRect(x0 <= x1 ? x0 : x1, y0,
					   x0 <= x1 ? x1 : x0, y1))
	  != splashClipAllOutside) {
	if (dx > 0) {
	  x2 = x0;
	  for (y = y0; y < y1; ++y) {
	    x3 = splashFloor(seg->x0 + (y + 1 - seg->y0) * dxdy);
	    drawSpan(x2, x3 - 1, y, state->strokePattern,
		     clipRes == splashClipAllInside);
	    x2 = x3;
	  }
	  drawSpan(x2, x1, y, state->strokePattern,
		   clipRes == splashClipAllInside);
	} else {
	  x2 = x0;
	  for (y = y0; y < y1; ++y) {
	    x3 = splashFloor(seg->x0 + (y + 1 - seg->y0) * dxdy);
	    drawSpan(x3 + 1, x2, y, state->strokePattern,
		     clipRes == splashClipAllInside);
	    x2 = x3;
	  }
	  drawSpan(x1, x2, y, state->strokePattern,
		   clipRes == splashClipAllInside);
	}
      }

    // segment with |dy| > |dx|
    } else {
      dxdy = seg->dxdy;
      if (y0 > y1) {
	t = y0; y0 = y1; y1 = t;
      }
      if ((clipRes = state->clip->testRect(x0 <= x1 ? x0 : x1, y0,
					   x0 <= x1 ? x1 : x0, y1))
	  != splashClipAllOutside) {
	for (y = y0; y <= y1; ++y) {
	  x = splashFloor(seg->x0 + (y - seg->y0) * dxdy);
	  drawPixel(x, y, state->strokePattern,
		    clipRes == splashClipAllInside);
	}
      }
    }
  }
}

void Splash::strokeWide(SplashXPath *xPath) {
  SplashXPathSeg *seg, *seg2;
  SplashPath *widePath;
  SplashCoord d, dx, dy, wdx, wdy, dxPrev, dyPrev, wdxPrev, wdyPrev;
  SplashCoord dotprod, miter;
  int i, j;

  dx = dy = wdx = wdy = 0; // make gcc happy
  dxPrev = dyPrev = wdxPrev = wdyPrev = 0; // make gcc happy

  for (i = 0, seg = xPath->segs; i < xPath->length; ++i, ++seg) {

    // save the deltas for the previous segment; if this is the first
    // segment on a subpath, compute the deltas for the last segment
    // on the subpath (which may be used to draw a line join)
    if (seg->flags & splashXPathFirst) {
      for (j = i + 1, seg2 = &xPath->segs[j]; j < xPath->length; ++j, ++seg2) {
	if (seg2->flags & splashXPathLast) {
	  d = splashDist(seg2->x0, seg2->y0, seg2->x1, seg2->y1);
	  if (d == 0) {
	    //~ not clear what the behavior should be for joins with d==0
	    dxPrev = 0;
	    dyPrev = 1;
	  } else {
	    d = 1 / d;
	    dxPrev = d * (seg2->x1 - seg2->x0);
	    dyPrev = d * (seg2->y1 - seg2->y0);
	  }
	  wdxPrev = 0.5 * state->lineWidth * dxPrev;
	  wdyPrev = 0.5 * state->lineWidth * dyPrev;
	  break;
	}
      }
    } else {
      dxPrev = dx;
      dyPrev = dy;
      wdxPrev = wdx;
      wdyPrev = wdy;
    }

    // compute deltas for this line segment
    d = splashDist(seg->x0, seg->y0, seg->x1, seg->y1);
    if (d == 0) {
      // we need to draw end caps on zero-length lines
      //~ not clear what the behavior should be for splashLineCapButt with d==0
      dx = 0;
      dy = 1;
    } else {
      d = 1 / d;
      dx = d * (seg->x1 - seg->x0);
      dy = d * (seg->y1 - seg->y0);
    }
    wdx = 0.5 * state->lineWidth * dx;
    wdy = 0.5 * state->lineWidth * dy;

    // initialize the path (which will be filled)
    widePath = new SplashPath();
    widePath->moveTo(seg->x0 - wdy, seg->y0 + wdx);

    // draw the start cap
    if (seg->flags & splashXPathEnd0) {
      switch (state->lineCap) {
      case splashLineCapButt:
	widePath->lineTo(seg->x0 + wdy, seg->y0 - wdx);
	break;
      case splashLineCapRound:
	widePath->arcCWTo(seg->x0 + wdy, seg->y0 - wdx, seg->x0, seg->y0);
	break;
      case splashLineCapProjecting:
	widePath->lineTo(seg->x0 - wdx - wdy, seg->y0 + wdx - wdy);
	widePath->lineTo(seg->x0 - wdx + wdy, seg->y0 - wdx - wdy);
	widePath->lineTo(seg->x0 + wdy, seg->y0 - wdx);
	break;
      }
    } else {
      widePath->lineTo(seg->x0 + wdy, seg->y0 - wdx);
    }

    // draw the left side of the segment
    widePath->lineTo(seg->x1 + wdy, seg->y1 - wdx);

    // draw the end cap
    if (seg->flags & splashXPathEnd1) {
      switch (state->lineCap) {
      case splashLineCapButt:
	widePath->lineTo(seg->x1 - wdy, seg->y1 + wdx);
	break;
      case splashLineCapRound:
	widePath->arcCWTo(seg->x1 - wdy, seg->y1 + wdx, seg->x1, seg->y1);
	break;
      case splashLineCapProjecting:
	widePath->lineTo(seg->x1 + wdx + wdy, seg->y1 - wdx + wdy);
	widePath->lineTo(seg->x1 + wdx - wdy, seg->y1 + wdx + wdy);
	widePath->lineTo(seg->x1 - wdy, seg->y1 + wdx);
	break;
      }
    } else {
      widePath->lineTo(seg->x1 - wdy, seg->y1 + wdx);
    }

    // draw the right side of the segment
    widePath->lineTo(seg->x0 - wdy, seg->y0 + wdx);

    // fill the segment
    fillWithPattern(widePath, gTrue, state->strokePattern);
    delete widePath;

    // draw the line join
    if (!(seg->flags & splashXPathEnd0)) {
      widePath = NULL;
      switch (state->lineJoin) {
      case splashLineJoinMiter:
	dotprod = -(dx * dxPrev + dy * dyPrev);
	if (fabs(fabs(dotprod) - 1) > 0.01) {
	  widePath = new SplashPath();
	  widePath->moveTo(seg->x0, seg->y0);
	  miter = 2 / (1 - dotprod);
	  if (splashSqrt(miter) <= state->miterLimit) {
	    miter = splashSqrt(miter - 1);
	    if (dy * dxPrev > dx * dyPrev) {
	      widePath->lineTo(seg->x0 + wdyPrev, seg->y0 - wdxPrev);
	      widePath->lineTo(seg->x0 + wdy - miter * wdx,
			       seg->y0 - wdx - miter * wdy);
	      widePath->lineTo(seg->x0 + wdy, seg->y0 - wdx);
	    } else {
	      widePath->lineTo(seg->x0 - wdyPrev, seg->y0 + wdxPrev);
	      widePath->lineTo(seg->x0 - wdy - miter * wdx,
			       seg->y0 + wdx - miter * wdy);
	      widePath->lineTo(seg->x0 - wdy, seg->y0 + wdx);
	    }
	  } else {
	    if (dy * dxPrev > dx * dyPrev) {
	      widePath->lineTo(seg->x0 + wdyPrev, seg->y0 - wdxPrev);
	      widePath->lineTo(seg->x0 + wdy, seg->y0 - wdx);
	    } else {
	      widePath->lineTo(seg->x0 - wdyPrev, seg->y0 + wdxPrev);
	      widePath->lineTo(seg->x0 - wdy, seg->y0 + wdx);
	    }
	  }
	}
	break;
      case splashLineJoinRound:
	widePath = new SplashPath();
	widePath->moveTo(seg->x0 + wdy, seg->y0 - wdx);
	widePath->arcCWTo(seg->x0 + wdy, seg->y0 - wdx, seg->x0, seg->y0);
	break;
      case splashLineJoinBevel:
	widePath = new SplashPath();
	widePath->moveTo(seg->x0, seg->y0);
	if (dy * dxPrev > dx * dyPrev) {
	  widePath->lineTo(seg->x0 + wdyPrev, seg->y0 - wdxPrev);
	  widePath->lineTo(seg->x0 + wdy, seg->y0 - wdx);
	} else {
	  widePath->lineTo(seg->x0 - wdyPrev, seg->y0 + wdxPrev);
	  widePath->lineTo(seg->x0 - wdy, seg->y0 + wdx);
	}
	break;
      }
      if (widePath) {
	fillWithPattern(widePath, gTrue, state->strokePattern);
	delete widePath;
      }
    }
  }
}

SplashXPath *Splash::makeDashedPath(SplashXPath *xPath) {
  SplashXPath *dPath;
  GBool lineDashStartOn, lineDashOn;
  GBool atSegStart, atSegEnd, atDashStart, atDashEnd;
  int lineDashStartIdx, lineDashIdx, subpathStart;
  SplashCoord lineDashTotal, lineDashStartPhase, lineDashDist;
  int segIdx;
  SplashXPathSeg *seg;
  SplashCoord sx0, sy0, sx1, sy1, ax0, ay0, ax1, ay1, dist;
  int i;

  dPath = new SplashXPath();

  lineDashTotal = 0;
  for (i = 0; i < state->lineDashLength; ++i) {
    lineDashTotal += state->lineDash[i];
  }
  lineDashStartPhase = state->lineDashPhase;
  i = splashFloor(lineDashStartPhase / lineDashTotal);
  lineDashStartPhase -= i * lineDashTotal;
  lineDashStartOn = gTrue;
  lineDashStartIdx = 0;
  while (lineDashStartPhase >= state->lineDash[lineDashStartIdx]) {
    lineDashStartOn = !lineDashStartOn;
    lineDashStartPhase -= state->lineDash[lineDashStartIdx];
    ++lineDashStartIdx;
  }

  segIdx = 0;
  seg = xPath->segs;
  sx0 = seg->x0;
  sy0 = seg->y0;
  sx1 = seg->x1;
  sy1 = seg->y1;
  dist = splashDist(sx0, sy0, sx1, sy1);
  lineDashOn = lineDashStartOn;
  lineDashIdx = lineDashStartIdx;
  lineDashDist = state->lineDash[lineDashIdx] - lineDashStartPhase;
  atSegStart = gTrue;
  atDashStart = gTrue;
  subpathStart = dPath->length;

  while (segIdx < xPath->length) {

    ax0 = sx0;
    ay0 = sy0;
    if (dist <= lineDashDist) {
      ax1 = sx1;
      ay1 = sy1;
      lineDashDist -= dist;
      dist = 0;
      atSegEnd = gTrue;
      atDashEnd = lineDashDist == 0 || (seg->flags & splashXPathLast);
    } else {
      ax1 = sx0 + (lineDashDist / dist) * (sx1 - sx0);
      ay1 = sy0 + (lineDashDist / dist) * (sy1 - sy0);
      sx0 = ax1;
      sy0 = ay1;
      dist -= lineDashDist;
      lineDashDist = 0;
      atSegEnd = gFalse;
      atDashEnd = gTrue;
    }

    if (lineDashOn) {
      dPath->addSegment(ax0, ay0, ax1, ay1,
			atDashStart, atDashEnd,
			atDashStart, atDashEnd);
      // end of closed subpath
      if (atSegEnd &&
	  (seg->flags & splashXPathLast) &&
	  !(seg->flags & splashXPathEnd1)) {
	dPath->segs[subpathStart].flags &= ~splashXPathEnd0;
	dPath->segs[dPath->length - 1].flags &= ~splashXPathEnd1;
      }
    }

    if (atDashEnd) {
      lineDashOn = !lineDashOn;
      if (++lineDashIdx == state->lineDashLength) {
	lineDashIdx = 0;
      }
      lineDashDist = state->lineDash[lineDashIdx];
      atDashStart = gTrue;
    } else {
      atDashStart = gFalse;
    }
    if (atSegEnd) {
      if (++segIdx < xPath->length) {
	++seg;
	sx0 = seg->x0;
	sy0 = seg->y0;
	sx1 = seg->x1;
	sy1 = seg->y1;
	dist = splashDist(sx0, sy0, sx1, sy1);
	if (seg->flags & splashXPathFirst) {
	  lineDashOn = lineDashStartOn;
	  lineDashIdx = lineDashStartIdx;
	  lineDashDist = state->lineDash[lineDashIdx] - lineDashStartPhase;
	  atDashStart = gTrue;
	  subpathStart = dPath->length;
	}
      }
      atSegStart = gTrue;
    } else {
      atSegStart = gFalse;
    }
  }

  return dPath;
}

SplashError Splash::fill(SplashPath *path, GBool eo) {
  if (debugMode) {
    printf("fill [eo:%d]:\n", eo);
    dumpPath(path);
  }
  return fillWithPattern(path, eo, state->fillPattern);
}

SplashError Splash::fillWithPattern(SplashPath *path, GBool eo,
				    SplashPattern *pattern) {
  SplashXPath *xPath;
  SplashXPathScanner *scanner;
  int xMinI, yMinI, xMaxI, yMaxI, x0, x1, y;
  SplashClipResult clipRes, clipRes2;

  if (path->length == 0) {
    return splashErrEmptyPath;
  }
  xPath = new SplashXPath(path, state->flatness, gTrue);
  xPath->sort();
  scanner = new SplashXPathScanner(xPath, eo);

  // get the min and max x and y values
  scanner->getBBox(&xMinI, &yMinI, &xMaxI, &yMaxI);

  // check clipping
  if ((clipRes = state->clip->testRect(xMinI, yMinI, xMaxI, yMaxI))
      != splashClipAllOutside) {

    // draw the spans
    for (y = yMinI; y <= yMaxI; ++y) {
      while (scanner->getNextSpan(y, &x0, &x1)) {
	if (clipRes == splashClipAllInside) {
	  drawSpan(x0, x1, y, pattern, gTrue);
	} else {
	  clipRes2 = state->clip->testSpan(x0, x1, y);
	  drawSpan(x0, x1, y, pattern, clipRes2 == splashClipAllInside);
	}
      }
    }
  }

  delete scanner;
  delete xPath;
  return splashOk;
}

SplashError Splash::xorFill(SplashPath *path, GBool eo) {
  SplashXPath *xPath;
  SplashXPathScanner *scanner;
  int xMinI, yMinI, xMaxI, yMaxI, x0, x1, y;
  SplashClipResult clipRes, clipRes2;

  if (path->length == 0) {
    return splashErrEmptyPath;
  }
  xPath = new SplashXPath(path, state->flatness, gTrue);
  xPath->sort();
  scanner = new SplashXPathScanner(xPath, eo);

  // get the min and max x and y values
  scanner->getBBox(&xMinI, &yMinI, &xMaxI, &yMaxI);

  // check clipping
  if ((clipRes = state->clip->testRect(xMinI, yMinI, xMaxI, yMaxI))
      != splashClipAllOutside) {

    // draw the spans
    for (y = yMinI; y <= yMaxI; ++y) {
      while (scanner->getNextSpan(y, &x0, &x1)) {
	if (clipRes == splashClipAllInside) {
	  xorSpan(x0, x1, y, state->fillPattern, gTrue);
	} else {
	  clipRes2 = state->clip->testSpan(x0, x1, y);
	  xorSpan(x0, x1, y, state->fillPattern,
		  clipRes2 == splashClipAllInside);
	}
      }
    }
  }

  delete scanner;
  delete xPath;
  return splashOk;
}

void Splash::drawPixel(int x, int y, SplashColor *color, GBool noClip) {
  SplashMono1P *mono1;
  SplashBGR8P *bgr8;

  if (noClip || state->clip->test(x, y)) {
    switch (bitmap->mode) {
    case splashModeMono1:
      mono1 = &bitmap->data.mono8[y * bitmap->rowSize + (x >> 3)];
      if (color->mono1) {
	*mono1 |= 0x80 >> (x & 7);
      } else {
	*mono1 &= ~(0x80 >> (x & 7));
      }
      break;
    case splashModeMono8:
      bitmap->data.mono8[y * bitmap->rowSize + x] = color->mono8;
      break;
    case splashModeRGB8:
      bitmap->data.rgb8[y * (bitmap->rowSize >> 2) + x] = color->rgb8;
      break;
    case splashModeBGR8Packed:
      bgr8 = &bitmap->data.bgr8[y * bitmap->rowSize + 3 * x];
      bgr8[2] = splashBGR8R(color->bgr8);
      bgr8[1] = splashBGR8G(color->bgr8);
      bgr8[0] = splashBGR8B(color->bgr8);
      break;
    }
  }
}

void Splash::drawPixel(int x, int y, SplashPattern *pattern, GBool noClip) {
  SplashColor color;
  SplashMono1P *mono1;
  SplashBGR8P *bgr8;

  if (noClip || state->clip->test(x, y)) {
    color = pattern->getColor(x, y);
    switch (bitmap->mode) {
    case splashModeMono1:
      mono1 = &bitmap->data.mono8[y * bitmap->rowSize + (x >> 3)];
      if (color.mono1) {
	*mono1 |= 0x80 >> (x & 7);
      } else {
	*mono1 &= ~(0x80 >> (x & 7));
      }
      break;
    case splashModeMono8:
      bitmap->data.mono8[y * bitmap->rowSize + x] = color.mono8;
      break;
    case splashModeRGB8:
      bitmap->data.rgb8[y * (bitmap->rowSize >> 2) + x] = color.rgb8;
      break;
    case splashModeBGR8Packed:
      bgr8 = &bitmap->data.bgr8[y * bitmap->rowSize + 3 * x];
      bgr8[2] = splashBGR8R(color.bgr8);
      bgr8[1] = splashBGR8G(color.bgr8);
      bgr8[0] = splashBGR8B(color.bgr8);
      break;
    }
  }
}

void Splash::drawSpan(int x0, int x1, int y, SplashPattern *pattern,
		      GBool noClip) {
  SplashColor color;
  SplashMono1P *mono1;
  SplashMono8 *mono8;
  SplashRGB8 *rgb8;
  SplashBGR8P *bgr8;
  SplashMono1 mask1;
  int i, j, n;

  n = x1 - x0 + 1;

  switch (bitmap->mode) {
  case splashModeMono1:
    mono1 = &bitmap->data.mono8[y * bitmap->rowSize + (x0 >> 3)];
    i = 0;
    if (pattern->isStatic()) {
      color = pattern->getColor(0, 0);
      if ((j = x0 & 7)) {
	mask1 = 0x80 >> j;
	for (; j < 8 && i < n; ++i, ++j) {
	  if (noClip || state->clip->test(x0 + i, y)) {
	    if (color.mono1) {
	      *mono1 |= mask1;
	    } else {
	      *mono1 &= ~mask1;
	    }
	  }
	  mask1 >>= 1;
	}
	++mono1;
      }
      while (i < n) {
	mask1 = 0x80;
	for (j = 0; j < 8 && i < n; ++i, ++j) {
	  if (noClip || state->clip->test(x0 + i, y)) {
	    if (color.mono1) {
	      *mono1 |= mask1;
	    } else {
	      *mono1 &= ~mask1;
	    }
	  }
	  mask1 >>= 1;
	}
	++mono1;
      }
    } else {
      if ((j = x0 & 7)) {
	mask1 = 0x80 >> j;
	for (; j < 8 && i < n; ++i, ++j) {
	  if (noClip || state->clip->test(x0 + i, y)) {
	    color = pattern->getColor(x0 + i, y);
	    if (color.mono1) {
	      *mono1 |= mask1;
	    } else {
	      *mono1 &= ~mask1;
	    }
	  }
	  mask1 >>= 1;
	}
	++mono1;
      }
      while (i < n) {
	mask1 = 0x80;
	for (j = 0; j < 8 && i < n; ++i, ++j) {
	  if (noClip || state->clip->test(x0 + i, y)) {
	    color = pattern->getColor(x0 + i, y);
	    if (color.mono1) {
	      *mono1 |= mask1;
	    } else {
	      *mono1 &= ~mask1;
	    }
	  }
	  mask1 >>= 1;
	}
	++mono1;
      }
    }
    break;

  case splashModeMono8:
    mono8 = &bitmap->data.mono8[y * bitmap->rowSize + x0];
    if (pattern->isStatic()) {
      color = pattern->getColor(0, 0);
      for (i = 0; i < n; ++i) {
	if (noClip || state->clip->test(x0 + i, y)) {
	  *mono8 = color.mono8;
	}
	++mono8;
      }
    } else {
      for (i = 0; i < n; ++i) {
	if (noClip || state->clip->test(x0 + i, y)) {
	  color = pattern->getColor(x0 + i, y);
	  *mono8 = color.mono8;
	}
	++mono8;
      }
    }
    break;

  case splashModeRGB8:
    rgb8 = &bitmap->data.rgb8[y * (bitmap->rowSize >> 2) + x0];
    if (pattern->isStatic()) {
      color = pattern->getColor(0, 0);
      for (i = 0; i < n; ++i) {
	if (noClip || state->clip->test(x0 + i, y)) {
	  *rgb8 = color.rgb8;
	}
	++rgb8;
      }
    } else {
      for (i = 0; i < n; ++i) {
	if (noClip || state->clip->test(x0 + i, y)) {
	  color = pattern->getColor(x0 + i, y);
	  *rgb8 = color.rgb8;
	}
	++rgb8;
      }
    }
    break;

  case splashModeBGR8Packed:
    bgr8 = &bitmap->data.bgr8[y * bitmap->rowSize + 3 * x0];
    if (pattern->isStatic()) {
      color = pattern->getColor(0, 0);
      for (i = 0; i < n; ++i) {
	if (noClip || state->clip->test(x0 + i, y)) {
	  bgr8[2] = splashBGR8R(color.bgr8);
	  bgr8[1] = splashBGR8G(color.bgr8);
	  bgr8[0] = splashBGR8B(color.bgr8);
	}
	bgr8 += 3;
      }
    } else {
      for (i = 0; i < n; ++i) {
	if (noClip || state->clip->test(x0 + i, y)) {
	  color = pattern->getColor(x0 + i, y);
	  bgr8[2] = splashBGR8R(color.bgr8);
	  bgr8[1] = splashBGR8G(color.bgr8);
	  bgr8[0] = splashBGR8B(color.bgr8);
	}
	bgr8 += 3;
      }
    }
    break;
  }
}

void Splash::xorSpan(int x0, int x1, int y, SplashPattern *pattern,
		     GBool noClip) {
  SplashColor color;
  SplashMono1P *mono1;
  SplashMono8 *mono8;
  SplashRGB8 *rgb8;
  SplashBGR8P *bgr8;
  SplashMono1 mask1;
  int i, j, n;

  n = x1 - x0 + 1;

  switch (bitmap->mode) {
  case splashModeMono1:
    mono1 = &bitmap->data.mono8[y * bitmap->rowSize + (x0 >> 3)];
    i = 0;
    if ((j = x0 & 7)) {
      mask1 = 0x80 >> j;
      for (j = x0 & 7; j < 8 && i < n; ++i, ++j) {
	if (noClip || state->clip->test(x0 + i, y)) {
	  color = pattern->getColor(x0 + i, y);
	  if (color.mono1) {
	    *mono1 ^= mask1;
	  }
	}
	mask1 >>= 1;
      }
      ++mono1;
    }
    while (i < n) {
      mask1 = 0x80;
      for (j = 0; j < 8 && i < n; ++i, ++j) {
	if (noClip || state->clip->test(x0 + i, y)) {
	  color = pattern->getColor(x0 + i, y);
	  if (color.mono1) {
	    *mono1 ^= mask1;
	  }
	}
	mask1 >>= 1;
      }
      ++mono1;
    }
    break;

  case splashModeMono8:
    mono8 = &bitmap->data.mono8[y * bitmap->rowSize + x0];
    for (i = 0; i < n; ++i) {
      if (noClip || state->clip->test(x0 + i, y)) {
	color = pattern->getColor(x0 + i, y);
	*mono8 ^= color.mono8;
      }
      ++mono8;
    }
    break;

  case splashModeRGB8:
    rgb8 = &bitmap->data.rgb8[y * (bitmap->rowSize >> 2) + x0];
    for (i = 0; i < n; ++i) {
      if (noClip || state->clip->test(x0 + i, y)) {
	color = pattern->getColor(x0 + i, y);
	*rgb8 ^= color.rgb8;
      }
      ++rgb8;
    }
    break;

  case splashModeBGR8Packed:
    bgr8 = &bitmap->data.bgr8[y * bitmap->rowSize + 3 * x0];
    for (i = 0; i < n; ++i) {
      if (noClip || state->clip->test(x0 + i, y)) {
	color = pattern->getColor(x0 + i, y);
	bgr8[2] ^= splashBGR8R(color.bgr8);
	bgr8[1] ^= splashBGR8G(color.bgr8);
	bgr8[0] ^= splashBGR8B(color.bgr8);
      }
      bgr8 += 3;
    }
    break;
  }
}

void Splash::getPixel(int x, int y, SplashColor *pixel) {
  SplashBGR8P *bgr8;

  if (y < 0 || y >= bitmap->height || x < 0 || x >= bitmap->width) {
    return;
  }
  switch (bitmap->mode) {
  case splashModeMono1:
    pixel->mono1 = (bitmap->data.mono1[y * bitmap->rowSize + (x >> 3)]
		    >> (7 - (x & 7))) & 1;
    break;
  case splashModeMono8:
    pixel->mono8 = bitmap->data.mono8[y * bitmap->rowSize + x];
    break;
  case splashModeRGB8:
    pixel->rgb8 = bitmap->data.rgb8[y * (bitmap->rowSize >> 2) + x];
    break;
  case splashModeBGR8Packed:
    bgr8 = &bitmap->data.bgr8[y * bitmap->rowSize + 3 * x];
    pixel->bgr8 = splashMakeBGR8(bgr8[2], bgr8[1], bgr8[0]);
    break;
  }
}

SplashError Splash::fillChar(SplashCoord x, SplashCoord y,
			     int c, SplashFont *font) {
  SplashGlyphBitmap glyph;
  int x0, y0, xFrac, yFrac;
  SplashError err;

  if (debugMode) {
    printf("fillChar: x=%.2f y=%.2f c=%3d=0x%02x='%c'\n",
	   x, y, c, c, c);
  }
  x0 = splashFloor(x);
  xFrac = splashFloor((x - x0) * splashFontFraction);
  y0 = splashFloor(y);
  yFrac = splashFloor((y - y0) * splashFontFraction);
  if (!font->getGlyph(c, xFrac, yFrac, &glyph)) {
    return splashErrNoGlyph;
  }
  err = fillGlyph(x, y, &glyph);
  if (glyph.freeData) {
    gfree(glyph.data);
  }
  return err;
}

SplashError Splash::fillGlyph(SplashCoord x, SplashCoord y,
			      SplashGlyphBitmap *glyph) {
  int alpha, ialpha;
  Guchar *p;
  SplashColor fg;
  SplashMono1P *mono1Ptr;
  SplashMono8 *mono8Ptr;
  SplashRGB8 *rgb8Ptr;
  SplashBGR8P *bgr8Ptr;
  SplashMono8 bgMono8;
  int bgR, bgG, bgB;
  SplashClipResult clipRes;
  GBool noClip;
  int x0, y0, x1, y1, xx, xx1, yy;

  x0 = splashFloor(x);
  y0 = splashFloor(y);

  if ((clipRes = state->clip->testRect(x0 - glyph->x,
				       y0 - glyph->y,
				       x0 - glyph->x + glyph->w - 1,
				       y0 - glyph->y + glyph->h - 1))
      != splashClipAllOutside) {
    noClip = clipRes == splashClipAllInside;

    //~ optimize this
    if (glyph->aa) {
      p = glyph->data;
      for (yy = 0, y1 = y0 - glyph->y; yy < glyph->h; ++yy, ++y1) {
	for (xx = 0, x1 = x0 - glyph->x; xx < glyph->w; ++xx, ++x1) {
	  alpha = *p++;
	  if (alpha > 0) {
	    if (noClip || state->clip->test(x1, y1)) {
	      ialpha = 255 - alpha;
	      fg = state->fillPattern->getColor(x1, y1);
	      switch (bitmap->mode) {
	      case splashModeMono1:
		if (alpha >= 0x80) {
		  mono1Ptr = &bitmap->data.mono1[y1 * bitmap->rowSize +
						 (x1 >> 3)];
		  if (fg.mono1) {
		    *mono1Ptr |= 0x80 >> (x1 & 7);
		  } else {
		    *mono1Ptr &= ~(0x80 >> (x1 & 7));
		  }
		}
		break;
	      case splashModeMono8:
		mono8Ptr = &bitmap->data.mono8[y1 * bitmap->rowSize + x1];
		bgMono8 = *mono8Ptr;
		// note: floor(x / 255) = x >> 8 (for 16-bit x)
		*mono8Ptr = (alpha * fg.mono8 + ialpha * bgMono8) >> 8;
		break;
	      case splashModeRGB8:
		rgb8Ptr = &bitmap->data.rgb8[y1 * (bitmap->rowSize >> 2) + x1];
		bgR = splashRGB8R(*rgb8Ptr);
		bgG = splashRGB8G(*rgb8Ptr);
		bgB = splashRGB8B(*rgb8Ptr);
		*rgb8Ptr = splashMakeRGB8((alpha * splashRGB8R(fg.rgb8) +
					   ialpha * bgR) >> 8,
					  (alpha * splashRGB8G(fg.rgb8) +
					   ialpha * bgG) >> 8,
					  (alpha * splashRGB8B(fg.rgb8) +
					   ialpha * bgB) >> 8);
		break;
	      case splashModeBGR8Packed:
		bgr8Ptr = &bitmap->data.bgr8[y1 * bitmap->rowSize + 3 * x1];
		bgr8Ptr[2] =
		    (alpha * splashBGR8R(fg.bgr8) + ialpha * bgr8Ptr[2]) >> 8;
		bgr8Ptr[1] =
		    (alpha * splashBGR8G(fg.bgr8) + ialpha * bgr8Ptr[1]) >> 8;
		bgr8Ptr[0] =
		    (alpha * splashBGR8B(fg.bgr8) + ialpha * bgr8Ptr[0]) >> 8;
		break;
	      }
	    }
	  }
	}
      }

    } else {
      p = glyph->data;
      for (yy = 0, y1 = y0 - glyph->y; yy < glyph->h; ++yy, ++y1) {
	for (xx = 0, x1 = x0 - glyph->x; xx < glyph->w; xx += 8) {
	  alpha = *p++;
	  for (xx1 = 0; xx1 < 8 && xx + xx1 < glyph->w; ++xx1, ++x1) {
	    if (alpha & 0x80) {
	      if (noClip || state->clip->test(x1, y1)) {
		fg = state->fillPattern->getColor(x1, y1);
		switch (bitmap->mode) {
		case splashModeMono1:
		  mono1Ptr = &bitmap->data.mono1[y1 * bitmap->rowSize +
						 (x1 >> 3)];
		  if (fg.mono1) {
		    *mono1Ptr |= 0x80 >> (x1 & 7);
		  } else {
		    *mono1Ptr &= ~(0x80 >> (x1 & 7));
		  }
		  break;
		case splashModeMono8:
		  bitmap->data.mono8[y1 * bitmap->width + x1] = fg.mono8;
		  break;
		case splashModeRGB8:
		  bitmap->data.rgb8[y1 * bitmap->width + x1] = fg.rgb8;
		  break;
		case splashModeBGR8Packed:
		  bgr8Ptr = &bitmap->data.bgr8[y1 * bitmap->rowSize + 3 * x1];
		  bgr8Ptr[2] = splashBGR8R(fg.bgr8);
		  bgr8Ptr[1] = splashBGR8G(fg.bgr8);
		  bgr8Ptr[0] = splashBGR8B(fg.bgr8);
		  break;
		}
	      }
	    }
	    alpha <<= 1;
	  }
	}
      }
    }
  }

  return splashOk;
}

SplashError Splash::fillImageMask(SplashImageMaskSource src, void *srcData,
				  int w, int h, SplashCoord *mat) {
  GBool rot;
  SplashCoord xScale, yScale, xShear, yShear;
  int tx, ty, scaledWidth, scaledHeight, xSign, ySign;
  int ulx, uly, llx, lly, urx, ury, lrx, lry;
  int ulx1, uly1, llx1, lly1, urx1, ury1, lrx1, lry1;
  int xMin, xMax, yMin, yMax;
  SplashClipResult clipRes, clipRes2;
  int yp, yq, yt, yStep, lastYStep;
  int xp, xq, xt, xStep, xSrc;
  int k1, spanXMin, spanXMax, spanY;
  SplashMono1 *pixBuf;
  SplashMono1 *p;
  int pixAcc;
  SplashCoord alpha;
  SplashColor fg, bg, pix;
  int x, y, x1, y1, x2, y2;
  int n, m, i, j;

  if (debugMode) {
    printf("fillImageMask: w=%d h=%d mat=[%.2f %.2f %.2f %.2f %.2f %.2f]\n",
	   w, h, mat[0], mat[1], mat[2], mat[3], mat[4], mat[5]);
  }

  // check for singular matrix
  if (splashAbs(mat[0] * mat[3] - mat[1] * mat[2]) < 0.000001) {
    return splashErrSingularMatrix;
  }

  // compute scale, shear, rotation, translation parameters
  rot = splashAbs(mat[1]) > splashAbs(mat[0]);
  if (rot) {
    xScale = -mat[1];
    yScale = mat[2] - (mat[0] * mat[3]) / mat[1];
    xShear = -mat[3] / yScale;
    yShear = -mat[0] / mat[1];
  } else {
    xScale = mat[0];
    yScale = mat[3] - (mat[1] * mat[2]) / mat[0];
    xShear = mat[2] / yScale;
    yShear = mat[1] / mat[0];
  }
  tx = splashRound(mat[4]);
  ty = splashRound(mat[5]);
  scaledWidth = abs(splashRound(mat[4] + xScale) - tx) + 1;
  scaledHeight = abs(splashRound(mat[5] + yScale) - ty) + 1;
  xSign = (xScale < 0) ? -1 : 1;
  ySign = (yScale < 0) ? -1 : 1;

  // clipping
  ulx1 = 0;
  uly1 = 0;
  urx1 = xSign * (scaledWidth - 1);
  ury1 = splashRound(yShear * urx1);
  llx1 = splashRound(xShear * ySign * (scaledHeight - 1));
  lly1 = ySign * (scaledHeight - 1) + splashRound(yShear * llx1);
  lrx1 = xSign * (scaledWidth - 1) +
           splashRound(xShear * ySign * (scaledHeight - 1));
  lry1 = ySign * (scaledHeight - 1) + splashRound(yShear * lrx1);
  if (rot) {
    ulx = tx + uly1;    uly = ty - ulx1;
    urx = tx + ury1;    ury = ty - urx1;
    llx = tx + lly1;    lly = ty - llx1;
    lrx = tx + lry1;    lry = ty - lrx1;
  } else {
    ulx = tx + ulx1;    uly = ty + uly1;
    urx = tx + urx1;    ury = ty + ury1;
    llx = tx + llx1;    lly = ty + lly1;
    lrx = tx + lrx1;    lry = ty + lry1;
  }
  xMin = (ulx < urx) ? (ulx < llx) ? (ulx < lrx) ? ulx : lrx
                                   : (llx < lrx) ? llx : lrx
		     : (urx < llx) ? (urx < lrx) ? urx : lrx
                                   : (llx < lrx) ? llx : lrx;
  xMax = (ulx > urx) ? (ulx > llx) ? (ulx > lrx) ? ulx : lrx
                                   : (llx > lrx) ? llx : lrx
		     : (urx > llx) ? (urx > lrx) ? urx : lrx
                                   : (llx > lrx) ? llx : lrx;
  yMin = (uly < ury) ? (uly < lly) ? (uly < lry) ? uly : lry
                                   : (lly < lry) ? lly : lry
		     : (ury < lly) ? (ury < lry) ? ury : lry
                                   : (lly < lry) ? lly : lry;
  yMax = (uly > ury) ? (uly > lly) ? (uly > lry) ? uly : lry
                                   : (lly > lry) ? lly : lry
		     : (ury > lly) ? (ury > lry) ? ury : lry
                                   : (lly > lry) ? lly : lry;
  clipRes = state->clip->testRect(xMin, yMin, xMax, yMax);

  // compute Bresenham parameters for x and y scaling
  yp = h / scaledHeight;
  yq = h % scaledHeight;
  xp = w / scaledWidth;
  xq = w % scaledWidth;

  // allocate pixel buffer
  pixBuf = (SplashMono1 *)gmalloc((yp + 1) * w * sizeof(SplashMono1));

  // init y scale Bresenham
  yt = 0;
  lastYStep = 1;

  for (y = 0; y < scaledHeight; ++y) {

    // y scale Bresenham
    yStep = yp;
    yt += yq;
    if (yt >= scaledHeight) {
      yt -= scaledHeight;
      ++yStep;
    }

    // read row(s) from image
    n = (yp > 0) ? yStep : lastYStep;
    if (n > 0) {
      p = pixBuf;
      for (i = 0; i < n; ++i) {
	for (j = 0; j < w; ++j) {
	  (*src)(srcData, p++);
	}
      }
    }
    lastYStep = yStep;

    // loop-invariant constants
    k1 = splashRound(xShear * ySign * y);

    // clipping test
    if (clipRes != splashClipAllInside &&
	!rot &&
	splashRound(yShear * k1) ==
	  splashRound(yShear * (xSign * (scaledWidth - 1) + k1))) {
      if (xSign > 0) {
	spanXMin = tx + k1;
	spanXMax = spanXMin + (scaledWidth - 1);
      } else {
	spanXMax = tx + k1;
	spanXMin = spanXMax - (scaledWidth - 1);
      }
      spanY = ty + ySign * y + splashRound(xShear * ySign * y);
      clipRes2 = state->clip->testSpan(spanXMin, spanXMax, spanY);
      if (clipRes2 == splashClipAllOutside) {
	continue;
      }
    } else {
      clipRes2 = clipRes;
    }

    // init x scale Bresenham
    xt = 0;
    xSrc = 0;

    for (x = 0; x < scaledWidth; ++x) {

      // x scale Bresenham
      xStep = xp;
      xt += xq;
      if (xt >= scaledWidth) {
	xt -= scaledWidth;
	++xStep;
      }

      // x shear
      x1 = xSign * x + k1;

      // y shear
      y1 = ySign * y + splashRound(yShear * x1);

      // rotation
      if (rot) {
	x2 = y1;
	y2 = -x1;
      } else {
	x2 = x1;
	y2 = y1;
      }

      // compute the alpha value for (x,y) after the x and y scaling
      // operations
      n = yStep > 0 ? yStep : 1;
      m = xStep > 0 ? xStep : 1;
      p = pixBuf + xSrc;
      pixAcc = 0;
      for (i = 0; i < n; ++i) {
	for (j = 0; j < m; ++j) {
	  pixAcc += *p++;
	}
	p += w - m;
      }

      // blend fill color with background
      if (pixAcc != 0) {
	fg = state->fillPattern->getColor(tx + x2, ty + y2);
	if (pixAcc == n * m) {
	  pix = fg;
	} else {
	  getPixel(tx + x2, ty + y2, &bg);
	  alpha = (SplashCoord)pixAcc / (SplashCoord)(n * m);
	  switch (bitmap->mode) {
	  case splashModeMono1:
	    pix.mono1 = splashRound(alpha * fg.mono1 +
				    (1 - alpha) * bg.mono1);
	    break;
	  case splashModeMono8:
	    pix.mono8 = splashRound(alpha * fg.mono8 +
				    (1 - alpha) * bg.mono8);
	    break;
	  case splashModeRGB8:
	    pix.rgb8 = splashMakeRGB8(
			   splashRound(alpha * splashRGB8R(fg.rgb8) +
				       (1 - alpha) * splashRGB8R(bg.rgb8)),
			   splashRound(alpha * splashRGB8G(fg.rgb8) +
				       (1 - alpha) * splashRGB8G(bg.rgb8)),
			   splashRound(alpha * splashRGB8B(fg.rgb8) +
				       (1 - alpha) * splashRGB8B(bg.rgb8)));
	  case splashModeBGR8Packed:
	    pix.bgr8 = splashMakeBGR8(
			   splashRound(alpha * splashBGR8R(fg.bgr8) +
				       (1 - alpha) * splashBGR8R(bg.bgr8)),
			   splashRound(alpha * splashBGR8G(fg.bgr8) +
				       (1 - alpha) * splashBGR8G(bg.bgr8)),
			   splashRound(alpha * splashBGR8B(fg.bgr8) +
				       (1 - alpha) * splashBGR8B(bg.bgr8)));
	    break;
	  }
	}
	drawPixel(tx + x2, ty + y2, &pix, clipRes2 == splashClipAllInside);
      }

      // x scale Bresenham
      xSrc += xStep;
    }
  }

  // free memory
  gfree(pixBuf);

  return splashOk;
}

SplashError Splash::drawImage(SplashImageSource src, void *srcData,
			      SplashColorMode srcMode,
			      int w, int h, SplashCoord *mat) {
  GBool ok, rot, halftone;
  SplashCoord xScale, yScale, xShear, yShear;
  int tx, ty, scaledWidth, scaledHeight, xSign, ySign;
  int ulx, uly, llx, lly, urx, ury, lrx, lry;
  int ulx1, uly1, llx1, lly1, urx1, ury1, lrx1, lry1;
  int xMin, xMax, yMin, yMax;
  SplashClipResult clipRes, clipRes2;
  int yp, yq, yt, yStep, lastYStep;
  int xp, xq, xt, xStep, xSrc;
  int k1, spanXMin, spanXMax, spanY;
  SplashColor *pixBuf, *p;
  Guchar *alphaBuf, *q;
  SplashColor pix;
  SplashCoord pixAcc[splashMaxColorComps];
  int alphaAcc;
  SplashCoord pixMul, alphaMul, alpha;
  int x, y, x1, y1, x2, y2;
  int n, m, i, j;

  if (debugMode) {
    printf("drawImage: srcMode=%d w=%d h=%d mat=[%.2f %.2f %.2f %.2f %.2f %.2f]\n",
	   srcMode, w, h, mat[0], mat[1], mat[2], mat[3], mat[4], mat[5]);
  }

  // check color modes
  ok = gFalse; // make gcc happy
  switch (bitmap->mode) {
  case splashModeMono1:
    ok = srcMode == splashModeMono1 || srcMode == splashModeMono8;
    break;
  case splashModeMono8:
    ok = srcMode == splashModeMono8;
    break;
  case splashModeRGB8:
    ok = srcMode == splashModeRGB8;
    break;
  case splashModeBGR8Packed:
    ok = srcMode == splashModeBGR8Packed;
    break;
  }
  if (!ok) {
    return splashErrModeMismatch;
  }
  halftone = bitmap->mode == splashModeMono1 && srcMode == splashModeMono8;

  // check for singular matrix
  if (splashAbs(mat[0] * mat[3] - mat[1] * mat[2]) < 0.000001) {
    return splashErrSingularMatrix;
  }

  // compute scale, shear, rotation, translation parameters
  rot = splashAbs(mat[1]) > splashAbs(mat[0]);
  if (rot) {
    xScale = -mat[1];
    yScale = mat[2] - (mat[0] * mat[3]) / mat[1];
    xShear = -mat[3] / yScale;
    yShear = -mat[0] / mat[1];
  } else {
    xScale = mat[0];
    yScale = mat[3] - (mat[1] * mat[2]) / mat[0];
    xShear = mat[2] / yScale;
    yShear = mat[1] / mat[0];
  }
  tx = splashRound(mat[4]);
  ty = splashRound(mat[5]);
  scaledWidth = abs(splashRound(mat[4] + xScale) - tx) + 1;
  scaledHeight = abs(splashRound(mat[5] + yScale) - ty) + 1;
  xSign = (xScale < 0) ? -1 : 1;
  ySign = (yScale < 0) ? -1 : 1;

  // clipping
  ulx1 = 0;
  uly1 = 0;
  urx1 = xSign * (scaledWidth - 1);
  ury1 = splashRound(yShear * urx1);
  llx1 = splashRound(xShear * ySign * (scaledHeight - 1));
  lly1 = ySign * (scaledHeight - 1) + splashRound(yShear * llx1);
  lrx1 = xSign * (scaledWidth - 1) +
           splashRound(xShear * ySign * (scaledHeight - 1));
  lry1 = ySign * (scaledHeight - 1) + splashRound(yShear * lrx1);
  if (rot) {
    ulx = tx + uly1;    uly = ty - ulx1;
    urx = tx + ury1;    ury = ty - urx1;
    llx = tx + lly1;    lly = ty - llx1;
    lrx = tx + lry1;    lry = ty - lrx1;
  } else {
    ulx = tx + ulx1;    uly = ty + uly1;
    urx = tx + urx1;    ury = ty + ury1;
    llx = tx + llx1;    lly = ty + lly1;
    lrx = tx + lrx1;    lry = ty + lry1;
  }
  xMin = (ulx < urx) ? (ulx < llx) ? (ulx < lrx) ? ulx : lrx
                                   : (llx < lrx) ? llx : lrx
		     : (urx < llx) ? (urx < lrx) ? urx : lrx
                                   : (llx < lrx) ? llx : lrx;
  xMax = (ulx > urx) ? (ulx > llx) ? (ulx > lrx) ? ulx : lrx
                                   : (llx > lrx) ? llx : lrx
		     : (urx > llx) ? (urx > lrx) ? urx : lrx
                                   : (llx > lrx) ? llx : lrx;
  yMin = (uly < ury) ? (uly < lly) ? (uly < lry) ? uly : lry
                                   : (lly < lry) ? lly : lry
		     : (ury < lly) ? (ury < lry) ? ury : lry
                                   : (lly < lry) ? lly : lry;
  yMax = (uly > ury) ? (uly > lly) ? (uly > lry) ? uly : lry
                                   : (lly > lry) ? lly : lry
		     : (ury > lly) ? (ury > lry) ? ury : lry
                                   : (lly > lry) ? lly : lry;
  if ((clipRes = state->clip->testRect(xMin, yMin, xMax, yMax))
      == splashClipAllOutside) {
    return splashOk;
  }

  // compute Bresenham parameters for x and y scaling
  yp = h / scaledHeight;
  yq = h % scaledHeight;
  xp = w / scaledWidth;
  xq = w % scaledWidth;

  // allocate pixel buffer
  pixBuf = (SplashColor *)gmalloc((yp + 1) * w * sizeof(SplashColor));
  alphaBuf = (Guchar *)gmalloc((yp + 1) * w * sizeof(Guchar));

  // init y scale Bresenham
  yt = 0;
  lastYStep = 1;

  for (y = 0; y < scaledHeight; ++y) {

    // y scale Bresenham
    yStep = yp;
    yt += yq;
    if (yt >= scaledHeight) {
      yt -= scaledHeight;
      ++yStep;
    }

    // read row(s) from image
    n = (yp > 0) ? yStep : lastYStep;
    if (n > 0) {
      p = pixBuf;
      q = alphaBuf;
      for (i = 0; i < n; ++i) {
	for (j = 0; j < w; ++j) {
	  (*src)(srcData, p++, q++);
	}
      }
    }
    lastYStep = yStep;

    // loop-invariant constants
    k1 = splashRound(xShear * ySign * y);

    // clipping test
    if (clipRes != splashClipAllInside &&
	!rot &&
	splashRound(yShear * k1) ==
	  splashRound(yShear * (xSign * (scaledWidth - 1) + k1))) {
      if (xSign > 0) {
	spanXMin = tx + k1;
	spanXMax = spanXMin + (scaledWidth - 1);
      } else {
	spanXMax = tx + k1;
	spanXMin = spanXMax - (scaledWidth - 1);
      }
      spanY = ty + ySign * y + splashRound(xShear * ySign * y);
      clipRes2 = state->clip->testSpan(spanXMin, spanXMax, spanY);
      if (clipRes2 == splashClipAllOutside) {
	continue;
      }
    } else {
      clipRes2 = clipRes;
    }

    // init x scale Bresenham
    xt = 0;
    xSrc = 0;

    for (x = 0; x < scaledWidth; ++x) {

      // x scale Bresenham
      xStep = xp;
      xt += xq;
      if (xt >= scaledWidth) {
	xt -= scaledWidth;
	++xStep;
      }

      // x shear
      x1 = xSign * x + k1;

      // y shear
      y1 = ySign * y + splashRound(yShear * x1);

      // rotation
      if (rot) {
	x2 = y1;
	y2 = -x1;
      } else {
	x2 = x1;
	y2 = y1;
      }

      // compute the filtered pixel at (x,y) after the x and y scaling
      // operations
      n = yStep > 0 ? yStep : 1;
      m = xStep > 0 ? xStep : 1;
      p = pixBuf + xSrc;
      q = alphaBuf + xSrc;
      for (i = 0; i < splashMaxColorComps; ++i) {
	pixAcc[i] = 0;
      }
      alphaAcc = 0;
      for (i = 0; i < n; ++i) {
	for (j = 0; j < m; ++j) {
	  switch (srcMode) {
	  case splashModeMono1:
	    pixAcc[0] += p->mono1;
	    break;
	  case splashModeMono8:
	    pixAcc[0] += p->mono8;
	    break;
	  case splashModeRGB8:
	    pixAcc[0] += splashRGB8R(p->rgb8);
	    pixAcc[1] += splashRGB8G(p->rgb8);
	    pixAcc[2] += splashRGB8B(p->rgb8);
	    break;
	  case splashModeBGR8Packed:
	    pixAcc[0] += splashBGR8R(p->bgr8);
	    pixAcc[1] += splashBGR8G(p->bgr8);
	    pixAcc[2] += splashBGR8B(p->bgr8);
	    break;
	  }
	  ++p;
	  alphaAcc += *q++;
	}
	p += w - m;
	q += w - m;
      }
      alphaMul = 1 / (SplashCoord)(n * m);
      if (halftone) {
	pixMul = (SplashCoord)alphaMul / 256.0;
      } else {
	pixMul = alphaMul;
      }
      alpha = (SplashCoord)alphaAcc * alphaMul;

      //~ this should blend if 0 < alpha < 1
      if (alpha > 0.75) {

	// mono8 -> mono1 conversion, with halftoning
	if (halftone) {
	  pix.mono1 = state->screen->test(tx + x2, ty + y2,
					  pixAcc[0] * pixMul);

	// no conversion, no halftoning
	} else {
	  switch (bitmap->mode) {
	  case splashModeMono1:
	    pix.mono1 = splashRound(pixAcc[0] * pixMul);
	    break;
	  case splashModeMono8:
	    pix.mono8 = splashRound(pixAcc[0] * pixMul);
	    break;
	  case splashModeRGB8:
	    pix.rgb8 = splashMakeRGB8(splashRound(pixAcc[0] * pixMul),
				      splashRound(pixAcc[1] * pixMul),
				      splashRound(pixAcc[2] * pixMul));
	    break;
	  case splashModeBGR8Packed:
	    pix.bgr8 = splashMakeBGR8(splashRound(pixAcc[0] * pixMul),
				      splashRound(pixAcc[1] * pixMul),
				      splashRound(pixAcc[2] * pixMul));
	    break;
	  }
	}

	// set pixel
	drawPixel(tx + x2, ty + y2, &pix, clipRes2 == splashClipAllInside);
      }

      // x scale Bresenham
      xSrc += xStep;
    }
  }

  gfree(pixBuf);
  gfree(alphaBuf);

  return splashOk;
}

void Splash::dumpPath(SplashPath *path) {
  int i;

  for (i = 0; i < path->length; ++i) {
    printf("  %3d: x=%8.2f y=%8.2f%s%s%s%s%s\n",
	   i, path->pts[i].x, path->pts[i].y,
	   (path->flags[i] & splashPathFirst) ? " first" : "",
	   (path->flags[i] & splashPathLast) ? " last" : "",
	   (path->flags[i] & splashPathClosed) ? " closed" : "",
	   (path->flags[i] & splashPathCurve) ? " curve" : "",
	   (path->flags[i] & splashPathArcCW) ? " arcCW" : "");
  }
}

void Splash::dumpXPath(SplashXPath *path) {
  int i;

  for (i = 0; i < path->length; ++i) {
    printf("  %4d: x0=%8.2f y0=%8.2f x1=%8.2f y1=%8.2f %s%s%s%s%s%s%s\n",
	   i, path->segs[i].x0, path->segs[i].y0,
	   path->segs[i].x1, path->segs[i].y1,
	   (path->segs[i].flags	& splashXPathFirst) ? "F" : " ",
	   (path->segs[i].flags	& splashXPathLast) ? "L" : " ",
	   (path->segs[i].flags	& splashXPathEnd0) ? "0" : " ",
	   (path->segs[i].flags	& splashXPathEnd1) ? "1" : " ",
	   (path->segs[i].flags	& splashXPathHoriz) ? "H" : " ",
	   (path->segs[i].flags	& splashXPathVert) ? "V" : " ",
	   (path->segs[i].flags	& splashXPathFlip) ? "P" : " ");
  }
}
