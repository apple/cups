//========================================================================
//
// SplashMath.h
//
//========================================================================

#ifndef SPLASHMATH_H
#define SPLASHMATH_H

#include <config.h>
#include <math.h>
#include "SplashTypes.h"

static inline SplashCoord splashAbs(SplashCoord x) {
  return fabs(x);
}

static inline int splashFloor(SplashCoord x) {
  return (int)floor(x);
}

static inline int splashCeil(SplashCoord x) {
  return (int)ceil(x);
}

static inline int splashRound(SplashCoord x) {
  return (int)floor(x + 0.5);
}

static inline SplashCoord splashSqrt(SplashCoord x) {
  return sqrt(x);
}

static inline SplashCoord splashPow(SplashCoord x, SplashCoord y) {
  return pow(x, y);
}

static inline SplashCoord splashDist(SplashCoord x0, SplashCoord y0,
				     SplashCoord x1, SplashCoord y1) {
  SplashCoord dx, dy;
  dx = x1 - x0;
  dy = y1 - y0;
  return sqrt(dx * dx + dy * dy);
}

#endif
