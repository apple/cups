//========================================================================
//
// SplashPattern.cc
//
//========================================================================

#include <config.h>

#ifdef USE_GCC_PRAGMAS
#pragma implementation
#endif

#include "SplashMath.h"
#include "SplashScreen.h"
#include "SplashPattern.h"

//------------------------------------------------------------------------
// SplashPattern
//------------------------------------------------------------------------

SplashPattern::SplashPattern() {
}

SplashPattern::~SplashPattern() {
}

//------------------------------------------------------------------------
// SplashSolidColor
//------------------------------------------------------------------------

SplashSolidColor::SplashSolidColor(SplashColor colorA) {
  color = colorA;
}

SplashSolidColor::~SplashSolidColor() {
}

SplashColor SplashSolidColor::getColor(int x, int y) {
  return color;
}

//------------------------------------------------------------------------
// SplashHalftone
//------------------------------------------------------------------------

SplashHalftone::SplashHalftone(SplashColor color0A, SplashColor color1A,
			       SplashScreen *screenA, SplashCoord valueA) {
  color0 = color0A;
  color1 = color1A;
  screen = screenA;
  value = valueA;
}

SplashPattern *SplashHalftone::copy() {
  return new SplashHalftone(color0, color1, screen->copy(), value);
}

SplashHalftone::~SplashHalftone() {
  delete screen;
}

SplashColor SplashHalftone::getColor(int x, int y) {
  return screen->test(x, y, value) ? color1 : color0;
}

GBool SplashHalftone::isStatic() {
  return screen->isStatic(value);
}
