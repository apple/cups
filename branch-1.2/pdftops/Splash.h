//========================================================================
//
// Splash.h
//
//========================================================================

#ifndef SPLASH_H
#define SPLASH_H

#include <config.h>

#ifdef USE_GCC_PRAGMAS
#pragma interface
#endif

#include "SplashTypes.h"

class SplashBitmap;
class SplashGlyphBitmap;
class SplashState;
class SplashPattern;
class SplashScreen;
class SplashPath;
class SplashXPath;
class SplashClip;
class SplashFont;

//------------------------------------------------------------------------

// Retrieves the next pixel in an image mask.  Normally, fills in
// *<pixel> and returns true.  If the image stream is exhausted,
// returns false.
typedef GBool (*SplashImageMaskSource)(void *data, SplashMono1 *pixel);

// Retrieves the next pixel in an image.  Normally, fills in *<pixel>
// (pixel color) and *<alpha> (1 for opaque, 0 for transparent), and
// returns true.  If the image stream is exhausted, returns false.
typedef GBool (*SplashImageSource)(void *data, SplashColor *pixel,
				   Guchar *alpha);

//------------------------------------------------------------------------
// Splash
//------------------------------------------------------------------------

class Splash {
public:

  // Create a new rasterizer object.
  Splash(SplashBitmap *bitmapA);

  ~Splash();

  //----- state read

  SplashPattern *getStrokePattern();
  SplashPattern *getFillPattern();
  SplashScreen *getScreen();
  SplashCoord getLineWidth();
  int getLineCap();
  int getLineJoin();
  SplashCoord getMiterLimit();
  SplashCoord getFlatness();
  SplashCoord *getLineDash();
  int getLineDashLength();
  SplashCoord getLineDashPhase();
  SplashClip *getClip();

  //----- state write

  void setStrokePattern(SplashPattern *strokeColor);
  void setFillPattern(SplashPattern *fillColor);
  void setScreen(SplashScreen *screen);
  void setLineWidth(SplashCoord lineWidth);
  void setLineCap(int lineCap);
  void setLineJoin(int lineJoin);
  void setMiterLimit(SplashCoord miterLimit);
  void setFlatness(SplashCoord flatness);
  // the <lineDash> array will be copied
  void setLineDash(SplashCoord *lineDash, int lineDashLength,
		   SplashCoord lineDashPhase);
  void clipResetToRect(SplashCoord x0, SplashCoord y0,
		       SplashCoord x1, SplashCoord y1);
  SplashError clipToRect(SplashCoord x0, SplashCoord y0,
			 SplashCoord x1, SplashCoord y1);
  SplashError clipToPath(SplashPath *path, GBool eo);

  //----- state save/restore

  void saveState();
  SplashError restoreState();

  //----- drawing operations

  // Fill the bitmap with <color>.  This is not subject to clipping.
  void clear(SplashColor color);

  // Stroke a path using the current stroke pattern.
  SplashError stroke(SplashPath *path);

  // Fill a path using the current fill pattern.
  SplashError fill(SplashPath *path, GBool eo);

  // Fill a path, XORing with the current fill pattern.
  SplashError xorFill(SplashPath *path, GBool eo);

  // Draw a character, using the current fill pattern.
  SplashError fillChar(SplashCoord x, SplashCoord y, int c, SplashFont *font);

  // Draw a glyph, using the current fill pattern.  This function does
  // not free any data, i.e., it ignores glyph->freeData.
  SplashError fillGlyph(SplashCoord x, SplashCoord y,
			SplashGlyphBitmap *glyph);

  // Draws an image mask using the fill color.  This will read <w>*<h>
  // pixels from <src>, in raster order, starting with the top line.
  // "1" pixels will be drawn with the current fill color; "0" pixels
  // are transparent.  The matrix:
  //    [ mat[0] mat[1] 0 ]
  //    [ mat[2] mat[3] 0 ]
  //    [ mat[4] mat[5] 1 ]
  // maps a unit square to the desired destination for the image, in
  // PostScript style:
  //    [x' y' 1] = [x y 1] * mat
  // Note that the Splash y axis points downward, and the image source
  // is assumed to produce pixels in raster order, starting from the
  // top line.
  SplashError fillImageMask(SplashImageMaskSource src, void *srcData,
			    int w, int h, SplashCoord *mat);

  // Draw an image.  This will read <w>*<h> pixels from <src>, in
  // raster order, starting with the top line.  These pixels are
  // assumed to be in the source mode, <srcMode>.  The following
  // combinations of source and target modes are supported:
  //    source       target
  //    ------       ------
  //    Mono1        Mono1
  //    Mono8        Mono1   -- with dithering
  //    Mono8        Mono8
  //    RGB8         RGB8
  //    BGR8packed   BGR8Packed
  // The matrix behaves as for fillImageMask.
  SplashError drawImage(SplashImageSource src, void *srcData,
			SplashColorMode srcMode,
			int w, int h, SplashCoord *mat);

  //~ drawMaskedImage

  //----- misc

  // Return the associated bitmap.
  SplashBitmap *getBitmap() { return bitmap; }

  // Toggle debug mode on or off.
  void setDebugMode(GBool debugModeA) { debugMode = debugModeA; }

private:

  void strokeNarrow(SplashXPath *xPath);
  void strokeWide(SplashXPath *xPath);
  SplashXPath *makeDashedPath(SplashXPath *xPath);
  SplashError fillWithPattern(SplashPath *path, GBool eo,
			      SplashPattern *pattern);
  void drawPixel(int x, int y, SplashColor *color, GBool noClip);
  void drawPixel(int x, int y, SplashPattern *pattern, GBool noClip);
  void drawSpan(int x0, int x1, int y, SplashPattern *pattern, GBool noClip);
  void xorSpan(int x0, int x1, int y, SplashPattern *pattern, GBool noClip);
  void putPixel(int x, int y, SplashColor *pixel);
  void getPixel(int x, int y, SplashColor *pixel);
  void dumpPath(SplashPath *path);
  void dumpXPath(SplashXPath *path);

  SplashBitmap *bitmap;
  SplashState *state;
  GBool debugMode;
};

#endif
