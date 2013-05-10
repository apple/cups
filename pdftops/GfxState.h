//========================================================================
//
// GfxState.h
//
// Copyright 1996 Derek B. Noonburg
//
//========================================================================

#ifndef GFXSTATE_H
#define GFXSTATE_H

#ifdef __GNUC__
#pragma interface
#endif

#include "gtypes.h"

class Object;
class Function;
class GfxFont;

//------------------------------------------------------------------------
// LabParams
//------------------------------------------------------------------------

// Parameters for L*a*b* color spaces.
struct LabParams {
  double whiteX, whiteY, whiteZ;
  double aMin, aMax, bMin, bMax;
};

//------------------------------------------------------------------------
// GfxColor
//------------------------------------------------------------------------

class GfxColor {
public:

  GfxColor(): r(0), g(0), b(0) {}

  // Set color.
  void setGray(double gray);
  void setCMYK(double c, double m, double y, double k);
  void setRGB(double r1, double g1, double b1);
  void setLab(double L, double a, double bb, LabParams *params);

  // Accessors.
  double getR() { return r; }
  double getG() { return g; }
  double getB() { return b; }
  double getGray() { return 0.299 * r + 0.587 * g + 0.114 * b; }

private:

  double r, g, b;
};

//------------------------------------------------------------------------
// GfxColorSpace
//------------------------------------------------------------------------

enum GfxColorMode {
  colorGray, colorCMYK, colorRGB, colorLab
};

class GfxColorSpace {
public:

  // Construct a colorspace.
  GfxColorSpace(Object *colorSpace);

  // Construct a simple colorspace: DeviceGray, DeviceCMYK, or
  // DeviceRGB.
  GfxColorSpace(GfxColorMode mode1);

  // Destructor.
  ~GfxColorSpace();

  // Copy.
  GfxColorSpace *copy() { return new GfxColorSpace(this); }

  // Is color space valid?
  GBool isOk() { return ok; }

  // Get the color mode.
  GfxColorMode getMode() { return mode; }

  // Get number of components in pixels of this colorspace.
  int getNumPixelComps() { return (sepFunc || indexed) ? 1 : numComps; }

  // Get number of components in colors of this colorspace.
  int getNumColorComps() { return numComps; }

  // Return true if colorspace is indexed.
  GBool isIndexed() { return indexed; }

  // Return true for a separation colorspace.
  GBool isSeparation() { return sepFunc ? gTrue : gFalse; }

  // Get default ranges for the components.
  void getDefaultRanges(double *decodeLow, double *decodeRange, int maxPixel);

  // Get lookup table (only for indexed colorspaces).
  int getIndexHigh() { return indexHigh; }
  Guchar *getLookupVal(int i) { return lookup[i]; }

  // Convert a pixel to a color.
  void getColor(double x[4], GfxColor *color);

  // Get the L*a*b* color space parameters.
  LabParams *getLabParams() { return &labParams; }

private:

  Function *sepFunc;		// separation tint transform function
  GfxColorMode mode;		// color mode
  GBool indexed;		// set for indexed colorspaces
  int numComps;			// number of components in colors
  int indexHigh;		// max pixel for indexed colorspace
  Guchar (*lookup)[4];		// lookup table (only for indexed
				//   colorspaces)
  LabParams labParams;		// parameters for L*a*b* color space
  GBool ok;			// is color space valid?

  GfxColorSpace(GfxColorSpace *colorSpace);
  void setMode(Object *colorSpace);
};

//------------------------------------------------------------------------
// Function
//------------------------------------------------------------------------

class Function {
public:

  // Create a PDF function object.
  Function(Object *funcObj);

  ~Function();
  
  Function *copy() { return new Function(this); }

  GBool isOk() { return ok; }

  // Return size of input and output tuples.
  int getInputSize() { return m; }
  int getOutputSize() { return n; }

  // Transform an input tuple into an output tuple.
  void transform(double *in, double *out);

private:

  Function(Function *func);

  int m, n;			// size of input and output tuples
  double domain[1][2];		// min and max values for function domain
  double range[4][2];		// min and max values for function range
  int sampleSize[1];		// number of samples for each domain element
  double encode[1][2];		// min and max values for domain encoder
  double decode[4][2];		// min and max values for range decoder
  double *samples;		// the samples
  GBool ok;
};

//------------------------------------------------------------------------
// GfxImageColorMap
//------------------------------------------------------------------------

class GfxImageColorMap {
public:

  // Constructor.
  GfxImageColorMap(int bits1, Object *decode, GfxColorSpace *colorSpace1);

  // Destructor.
  ~GfxImageColorMap();

  // Is color map valid?
  GBool isOk() { return ok; }

  // Get the color space.
  GfxColorSpace *getColorSpace() { return colorSpace; }

  // Get stream decoding info.
  int getNumPixelComps() { return numComps; }
  int getBits() { return bits; }

  // Get decode table.
  double getDecodeLow(int i) { return decodeLow[i]; }
  double getDecodeHigh(int i) { return decodeLow[i] + decodeRange[i]; }

  // Convert a pixel to a color.
  void getColor(Guchar x[4], GfxColor *color);

private:

  GfxColorSpace *colorSpace;	// the image colorspace
  int bits;			// bits per component
  int numComps;			// number of components in a pixel
  GBool indexed;		// set for indexed color space
  GBool sep;			// set for separation colorspaces
  GfxColorMode mode;		// color mode
  double (*lookup)[4];		// lookup table
  double decodeLow[4];		// minimum values for each component
  double decodeRange[4];	// max - min value for each component
  GBool ok;
};

//------------------------------------------------------------------------
// GfxSubpath and GfxPath
//------------------------------------------------------------------------

class GfxSubpath {
public:

  // Constructor.
  GfxSubpath(double x1, double y1);

  // Destructor.
  ~GfxSubpath();

  // Copy.
  GfxSubpath *copy() { return new GfxSubpath(this); }

  // Get points.
  int getNumPoints() { return n; }
  double getX(int i) { return x[i]; }
  double getY(int i) { return y[i]; }
  GBool getCurve(int i) { return curve[i]; }

  // Get last point.
  double getLastX() { return x[n-1]; }
  double getLastY() { return y[n-1]; }

  // Add a line segment.
  void lineTo(double x1, double y1);

  // Add a Bezier curve.
  void curveTo(double x1, double y1, double x2, double y2,
	       double x3, double y3);

  // Close the subpath.
  void close();
  GBool isClosed() { return closed; }

private:

  double *x, *y;		// points
  GBool *curve;			// curve[i] => point i is a control point
				//   for a Bezier curve
  int n;			// number of points
  int size;			// size of x/y arrays
  GBool closed;			// set if path is closed

  GfxSubpath(GfxSubpath *subpath);
};

class GfxPath {
public:

  // Constructor.
  GfxPath();

  // Destructor.
  ~GfxPath();

  // Copy.
  GfxPath *copy()
    { return new GfxPath(justMoved, firstX, firstY, subpaths, n, size); }

  // Is there a current point?
  GBool isCurPt() { return n > 0 || justMoved; }

  // Is the path non-empty, i.e., is there at least one segment?
  GBool isPath() { return n > 0; }

  // Get subpaths.
  int getNumSubpaths() { return n; }
  GfxSubpath *getSubpath(int i) { return subpaths[i]; }

  // Get last point on last subpath.
  double getLastX() { return subpaths[n-1]->getLastX(); }
  double getLastY() { return subpaths[n-1]->getLastY(); }

  // Move the current point.
  void moveTo(double x, double y);

  // Add a segment to the last subpath.
  void lineTo(double x, double y);

  // Add a Bezier curve to the last subpath
  void curveTo(double x1, double y1, double x2, double y2,
	       double x3, double y3);

  // Close the last subpath.
  void close() { subpaths[n-1]->close(); }

private:

  GBool justMoved;		// set if a new subpath was just started
  double firstX, firstY;	// first point in new subpath
  GfxSubpath **subpaths;	// subpaths
  int n;			// number of subpaths
  int size;			// size of subpaths array

  GfxPath(GBool justMoved1, double firstX1, double firstY1,
	  GfxSubpath **subpaths1, int n1, int size1);
};

//------------------------------------------------------------------------
// GfxState
//------------------------------------------------------------------------

class GfxState {
public:

  // Construct a default GfxState, for a device with resolution <dpi>,
  // page box (<x1>,<y1>)-(<x2>,<y2>), page rotation <rotate>, and
  // coordinate system specified by <upsideDown>.
  GfxState(int dpi, double px1a, double py1a, double px2a, double py2a,
	   int rotate, GBool upsideDown);

  // Destructor.
  ~GfxState();

  // Copy.
  GfxState *copy() { return new GfxState(this); }

  // Accessors.
  double *getCTM() { return ctm; }
  double getX1() { return px1; }
  double getY1() { return py1; }
  double getX2() { return px2; }
  double getY2() { return py2; }
  double getPageWidth() { return pageWidth; }
  double getPageHeight() { return pageHeight; }
  GfxColor *getFillColor() { return &fillColor; }
  GfxColor *getStrokeColor() { return &strokeColor; }
  double getLineWidth() { return lineWidth; }
  void getLineDash(double **dash, int *length, double *start)
    { *dash = lineDash; *length = lineDashLength; *start = lineDashStart; }
  int getFlatness() { return flatness; }
  int getLineJoin() { return lineJoin; }
  int getLineCap() { return lineCap; }
  double getMiterLimit() { return miterLimit; }
  GfxFont *getFont() { return font; }
  double getFontSize() { return fontSize; }
  double *getTextMat() { return textMat; }
  double getCharSpace() { return charSpace; }
  double getWordSpace() { return wordSpace; }
  double getHorizScaling() { return horizScaling; }
  double getLeading() { return leading; }
  double getRise() { return rise; }
  int getRender() { return render; }
  GfxPath *getPath() { return path; }
  double getCurX() { return curX; }
  double getCurY() { return curY; }
  double getLineX() { return lineX; }
  double getLineY() { return lineY; }

  // Is there a current point/path?
  GBool isCurPt() { return path->isCurPt(); }
  GBool isPath() { return path->isPath(); }

  // Transforms.
  void transform(double x1, double y1, double *x2, double *y2)
    { *x2 = ctm[0] * x1 + ctm[2] * y1 + ctm[4];
      *y2 = ctm[1] * x1 + ctm[3] * y1 + ctm[5]; }
  void transformDelta(double x1, double y1, double *x2, double *y2)
    { *x2 = ctm[0] * x1 + ctm[2] * y1;
      *y2 = ctm[1] * x1 + ctm[3] * y1; }
  void textTransform(double x1, double y1, double *x2, double *y2)
    { *x2 = textMat[0] * x1 + textMat[2] * y1 + textMat[4];
      *y2 = textMat[1] * x1 + textMat[3] * y1 + textMat[5]; }
  void textTransformDelta(double x1, double y1, double *x2, double *y2)
    { *x2 = textMat[0] * x1 + textMat[2] * y1;
      *y2 = textMat[1] * x1 + textMat[3] * y1; }
  double transformWidth(double w);
  double getTransformedLineWidth()
    { return transformWidth(lineWidth); }
  double getTransformedFontSize();
  void getFontTransMat(double *m11, double *m12, double *m21, double *m22);

  // Change state parameters.
  void setCTM(double a, double b, double c,
	      double d, double e, double f);
  void concatCTM(double a, double b, double c,
		 double d, double e, double f);
  void setFillGray(double gray)
    { fillColor.setGray(gray); }
  void setFillCMYK(double c, double m, double y, double k)
    { fillColor.setCMYK(c, m, y, k); }
  void setFillRGB(double r, double g, double b)
    { fillColor.setRGB(r, g, b); }
  void setStrokeGray(double gray)
    { strokeColor.setGray(gray); }
  void setStrokeCMYK(double c, double m, double y, double k)
    { strokeColor.setCMYK(c, m, y, k); }
  void setStrokeRGB(double r, double g, double b)
    { strokeColor.setRGB(r, g, b); }
  void setFillColorSpace(GfxColorSpace *colorSpace);
  void setStrokeColorSpace(GfxColorSpace *colorSpace);
  void setFillColor(double x[4])
    { fillColorSpace->getColor(x, &fillColor); }
  void setStrokeColor(double x[4])
    { strokeColorSpace->getColor(x, &strokeColor); }
  void setLineWidth(double width)
    { lineWidth = width; }
  void setLineDash(double *dash, int length, double start);
  void setFlatness(int flatness1) { flatness = flatness1; }
  void setLineJoin(int lineJoin1) { lineJoin = lineJoin1; }
  void setLineCap(int lineCap1) { lineCap = lineCap1; }
  void setMiterLimit(double miterLimit1) { miterLimit = miterLimit1; }
  void setFont(GfxFont *font1, double fontSize1)
    { font = font1; fontSize = fontSize1; }
  void setTextMat(double a, double b, double c,
		  double d, double e, double f)
    { textMat[0] = a; textMat[1] = b; textMat[2] = c;
      textMat[3] = d; textMat[4] = e; textMat[5] = f; }
  void setCharSpace(double space)
    { charSpace = space; }
  void setWordSpace(double space)
    { wordSpace = space; }
  void setHorizScaling(double scale)
    { horizScaling = 0.01 * scale; }
  void setLeading(double leading1)
    { leading = leading1; }
  void setRise(double rise1)
    { rise = rise1; }
  void setRender(int render1)
    { render = render1; }

  // Add to path.
  void moveTo(double x, double y)
    { path->moveTo(curX = x, curY = y); }
  void lineTo(double x, double y)
    { path->lineTo(curX = x, curY = y); }
  void curveTo(double x1, double y1, double x2, double y2,
	       double x3, double y3)
    { path->curveTo(x1, y1, x2, y2, curX = x3, curY = y3); }
  void closePath()
    { path->close(); curX = path->getLastX(); curY = path->getLastY(); }
  void clearPath();

  // Text position.
  void textMoveTo(double tx, double ty)
    { lineX = tx; lineY = ty; textTransform(tx, ty, &curX, &curY); }
  void textShift(double tx);
  void textShift(double tx, double ty);

  // Push/pop GfxState on/off stack.
  GfxState *save();
  GfxState *restore();
  GBool hasSaves() { return saved != NULL; }

private:

  double ctm[6];		// coord transform matrix
  double px1, py1, px2, py2;	// page corners (user coords)
  double pageWidth, pageHeight;	// page size (pixels)

  GfxColorSpace *fillColorSpace;   // fill color space
  GfxColorSpace *strokeColorSpace; // stroke color space
  GfxColor fillColor;		// fill color
  GfxColor strokeColor;		// stroke color

  double lineWidth;		// line width
  double *lineDash;		// line dash
  int lineDashLength;
  double lineDashStart;
  int flatness;			// curve flatness
  int lineJoin;			// line join style
  int lineCap;			// line cap style
  double miterLimit;		// line miter limit

  GfxFont *font;		// font
  double fontSize;		// font size
  double textMat[6];		// text matrix
  double charSpace;		// character spacing
  double wordSpace;		// word spacing
  double horizScaling;		// horizontal scaling
  double leading;		// text leading
  double rise;			// text rise
  int render;			// text rendering mode

  GfxPath *path;		// array of path elements
  double curX, curY;		// current point (user coords)
  double lineX, lineY;		// start of current text line (text coords)

  GfxState *saved;		// next GfxState on stack

  GfxState(GfxState *state);
};

#endif
