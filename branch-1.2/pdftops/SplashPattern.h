//========================================================================
//
// SplashPattern.h
//
//========================================================================

#ifndef SPLASHPATTERN_H
#define SPLASHPATTERN_H

#include <config.h>

#ifdef USE_GCC_PRAGMAS
#pragma interface
#endif

#include "SplashTypes.h"

class SplashScreen;

//------------------------------------------------------------------------
// SplashPattern
//------------------------------------------------------------------------

class SplashPattern {
public:

  SplashPattern();

  virtual SplashPattern *copy() = 0;

  virtual ~SplashPattern();

  // Return the color value for a specific pixel.
  virtual SplashColor getColor(int x, int y) = 0;

  // Returns true if this pattern object will return the same color
  // value for all pixels.
  virtual GBool isStatic() = 0;

private:
};

//------------------------------------------------------------------------
// SplashSolidColor
//------------------------------------------------------------------------

class SplashSolidColor: public SplashPattern {
public:

  SplashSolidColor(SplashColor colorA);

  virtual SplashPattern *copy() { return new SplashSolidColor(color); }

  virtual ~SplashSolidColor();

  virtual SplashColor getColor(int x, int y);

  virtual GBool isStatic() { return gTrue; }

private:

  SplashColor color;
};

//------------------------------------------------------------------------
// SplashHalftone
//------------------------------------------------------------------------

class SplashHalftone: public SplashPattern {
public:

  SplashHalftone(SplashColor color0A, SplashColor color1A,
		 SplashScreen *screenA, SplashCoord valueA);

  virtual SplashPattern *copy();

  virtual ~SplashHalftone();

  virtual SplashColor getColor(int x, int y);

  virtual GBool isStatic();

private:

  SplashColor color0, color1;
  SplashScreen *screen;
  SplashCoord value;
};

#endif
