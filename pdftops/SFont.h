//========================================================================
//
// SFont.h
//
// Base class for font rasterizers.
//
//========================================================================

#ifndef SFONT_H
#define SFONT_H

#ifdef __GNUC__
#pragma interface
#endif

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include "gtypes.h"

//------------------------------------------------------------------------

class SFontEngine {
public:

  SFontEngine(Display *displayA, Visual *visualA, int depthA,
	      Colormap colormapA);
  virtual ~SFontEngine();

  // Use a TrueColor visual.  Pixel values are computed as:
  //
  //     (r << rShift) + (g << gShift) + (b << bShift)
  //
  // where r, g, and b are scaled to the ranges [0,rMax], [0,gMax],
  // and [0,bMax], respectively.
  virtual void useTrueColor(int rMaxA, int rShiftA, int gMaxA, int gShiftA,
			    int bMaxA, int bShiftA);

  // Use an RGB color cube.  <colors> is an array containing
  // <nRGB>*<nRGB>*<nRGB> pixel values in red,green,blue order, e.g.,
  // for <nRGB>=2, there will be 8 entries:
  //
  //        |--- colors[i] ---|
  //     i  red    green  blue
  //     -  -----  -----  -----
  //     0  0000   0000   0000
  //     1  0000   0000   ffff
  //     2  0000   ffff   0000
  //     3  0000   ffff   ffff
  //     4  ffff   0000   0000
  //     5  ffff   0000   ffff
  //     6  ffff   ffff   0000
  //     7  ffff   ffff   ffff
  //
  // The <colors> array is not copied and must remain valid for the
  // lifetime of this SFont object.
  virtual void useColorCube(Gulong *colorsA, int nRGBA);

protected:

  // Find the closest match to (<r>,<g>,<b>).
  Gulong findColor(int r, int g, int b);

  //----- X parameters
  Display *display;
  Visual *visual;
  int depth;
  Colormap colormap;

  GBool trueColor;		// true for TrueColor, false for RGB cube

  //----- TrueColor parameters
  int rMax, gMax, bMax;
  int rShift, gShift, bShift;

  //----- RGB color cube parameters
  Gulong *colors;
  int nRGB;
};

//------------------------------------------------------------------------

class SFontFile {
public:

  // A typical subclass will provide a constructor along the lines of:
  //
  //     SomeFontFile(SomeFontEngine *engine, char *fontFileName);
  SFontFile();

  virtual ~SFontFile();

private:
};

//------------------------------------------------------------------------

class SFont {
public:

  // A typical subclass will provide a constructor along the lines of:
  //
  //     SomeFont(SomeFontFile *fontFile, double *m);
  //
  // where <m> is a transform matrix consisting of four elements,
  // using the PostScript ordering conventions (without any
  // translation):
  //
  //   [x' y'] = [x y] * [m0 m1]
  //                     [m2 m3]
  //
  // This is the level at which fonts are cached, and so the font
  // cannot be transformed after it is created.
  SFont();

  virtual ~SFont();

  // Draw a character <c> at <x>,<y> in color (<r>,<g>,<b>).  The RGB
  // values should each be in the range [0,65535].  Draws into <d>,
  // clipped to the rectangle (0,0)-(<w>-1,<h>-1).  Returns true if
  // the character was drawn successfully.
  virtual GBool drawChar(Drawable d, int w, int h, GC gc,
			 int x, int y, int r, int g, int b, Gushort c) = 0;

protected:
};

#endif
