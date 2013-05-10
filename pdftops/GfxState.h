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
#include "Object.h"

class Array;
class Function;
class GfxFont;

//------------------------------------------------------------------------
// GfxColor
//------------------------------------------------------------------------

#define gfxColorMaxComps 8

struct GfxColor {
  double c[gfxColorMaxComps];
};

//------------------------------------------------------------------------
// GfxRGB
//------------------------------------------------------------------------

struct GfxRGB {
  double r, g, b;
};

//------------------------------------------------------------------------
// GfxCMYK
//------------------------------------------------------------------------

struct GfxCMYK {
  double c, m, y, k;
};

//------------------------------------------------------------------------
// GfxColorSpace
//------------------------------------------------------------------------

enum GfxColorSpaceMode {
  csDeviceGray,
  csCalGray,
  csDeviceRGB,
  csCalRGB,
  csDeviceCMYK,
  csLab,
  csICCBased,
  csIndexed,
  csSeparation,
  csDeviceN,
  csPattern
};

class GfxColorSpace {
public:

  GfxColorSpace();
  virtual ~GfxColorSpace();
  virtual GfxColorSpace *copy() = 0;
  virtual GfxColorSpaceMode getMode() = 0;

  // Construct a color space.  Returns NULL if unsuccessful.
  static GfxColorSpace *parse(Object *csObj);

  // Convert to gray, RGB, or CMYK.
  virtual void getGray(GfxColor *color, double *gray) = 0;
  virtual void getRGB(GfxColor *color, GfxRGB *rgb) = 0;
  virtual void getCMYK(GfxColor *color, GfxCMYK *cmyk) = 0;

  // Return the number of color components.
  virtual int getNComps() = 0;

  // Return the default ranges for each component, assuming an image
  // with a max pixel value of <maxImgPixel>.
  virtual void getDefaultRanges(double *decodeLow, double *decodeRange,
				int maxImgPixel);

private:
};

//------------------------------------------------------------------------
// GfxDeviceGrayColorSpace
//------------------------------------------------------------------------

class GfxDeviceGrayColorSpace: public GfxColorSpace {
public:

  GfxDeviceGrayColorSpace();
  virtual ~GfxDeviceGrayColorSpace();
  virtual GfxColorSpace *copy();
  virtual GfxColorSpaceMode getMode() { return csDeviceGray; }

  virtual void getGray(GfxColor *color, double *gray);
  virtual void getRGB(GfxColor *color, GfxRGB *rgb);
  virtual void getCMYK(GfxColor *color, GfxCMYK *cmyk);

  virtual int getNComps() { return 1; }

private:
};

//------------------------------------------------------------------------
// GfxCalGrayColorSpace
//------------------------------------------------------------------------

class GfxCalGrayColorSpace: public GfxColorSpace {
public:

  GfxCalGrayColorSpace();
  virtual ~GfxCalGrayColorSpace();
  virtual GfxColorSpace *copy();
  virtual GfxColorSpaceMode getMode() { return csCalGray; }

  // Construct a CalGray color space.  Returns NULL if unsuccessful.
  static GfxColorSpace *parse(Array *arr);

  virtual void getGray(GfxColor *color, double *gray);
  virtual void getRGB(GfxColor *color, GfxRGB *rgb);
  virtual void getCMYK(GfxColor *color, GfxCMYK *cmyk);

  virtual int getNComps() { return 1; }

  // CalGray-specific access.
  double getWhiteX() { return whiteX; }
  double getWhiteY() { return whiteY; }
  double getWhiteZ() { return whiteZ; }
  double getBlackX() { return blackX; }
  double getBlackY() { return blackY; }
  double getBlackZ() { return blackZ; }
  double getGamma() { return gamma; }

private:

  double whiteX, whiteY, whiteZ;    // white point
  double blackX, blackY, blackZ;    // black point
  double gamma;			    // gamma value
};

//------------------------------------------------------------------------
// GfxDeviceRGBColorSpace
//------------------------------------------------------------------------

class GfxDeviceRGBColorSpace: public GfxColorSpace {
public:

  GfxDeviceRGBColorSpace();
  virtual ~GfxDeviceRGBColorSpace();
  virtual GfxColorSpace *copy();
  virtual GfxColorSpaceMode getMode() { return csDeviceRGB; }

  virtual void getGray(GfxColor *color, double *gray);
  virtual void getRGB(GfxColor *color, GfxRGB *rgb);
  virtual void getCMYK(GfxColor *color, GfxCMYK *cmyk);

  virtual int getNComps() { return 3; }

private:
};

//------------------------------------------------------------------------
// GfxCalRGBColorSpace
//------------------------------------------------------------------------

class GfxCalRGBColorSpace: public GfxColorSpace {
public:

  GfxCalRGBColorSpace();
  virtual ~GfxCalRGBColorSpace();
  virtual GfxColorSpace *copy();
  virtual GfxColorSpaceMode getMode() { return csCalRGB; }

  // Construct a CalRGB color space.  Returns NULL if unsuccessful.
  static GfxColorSpace *parse(Array *arr);

  virtual void getGray(GfxColor *color, double *gray);
  virtual void getRGB(GfxColor *color, GfxRGB *rgb);
  virtual void getCMYK(GfxColor *color, GfxCMYK *cmyk);

  virtual int getNComps() { return 3; }

  // CalRGB-specific access.
  double getWhiteX() { return whiteX; }
  double getWhiteY() { return whiteY; }
  double getWhiteZ() { return whiteZ; }
  double getBlackX() { return blackX; }
  double getBlackY() { return blackY; }
  double getBlackZ() { return blackZ; }
  double getGammaR() { return gammaR; }
  double getGammaG() { return gammaG; }
  double getGammaB() { return gammaB; }
  double *getMatrix() { return m; }

private:

  double whiteX, whiteY, whiteZ;    // white point
  double blackX, blackY, blackZ;    // black point
  double gammaR, gammaG, gammaB;    // gamma values
  double m[9];			    // ABC -> XYZ transform matrix
};

//------------------------------------------------------------------------
// GfxDeviceCMYKColorSpace
//------------------------------------------------------------------------

class GfxDeviceCMYKColorSpace: public GfxColorSpace {
public:

  GfxDeviceCMYKColorSpace();
  virtual ~GfxDeviceCMYKColorSpace();
  virtual GfxColorSpace *copy();
  virtual GfxColorSpaceMode getMode() { return csDeviceCMYK; }

  virtual void getGray(GfxColor *color, double *gray);
  virtual void getRGB(GfxColor *color, GfxRGB *rgb);
  virtual void getCMYK(GfxColor *color, GfxCMYK *cmyk);

  virtual int getNComps() { return 4; }

private:
};

//------------------------------------------------------------------------
// GfxLabColorSpace
//------------------------------------------------------------------------

class GfxLabColorSpace: public GfxColorSpace {
public:

  GfxLabColorSpace();
  virtual ~GfxLabColorSpace();
  virtual GfxColorSpace *copy();
  virtual GfxColorSpaceMode getMode() { return csLab; }

  // Construct a Lab color space.  Returns NULL if unsuccessful.
  static GfxColorSpace *parse(Array *arr);

  virtual void getGray(GfxColor *color, double *gray);
  virtual void getRGB(GfxColor *color, GfxRGB *rgb);
  virtual void getCMYK(GfxColor *color, GfxCMYK *cmyk);

  virtual int getNComps() { return 3; }

  virtual void getDefaultRanges(double *decodeLow, double *decodeRange,
				int maxImgPixel);

  // Lab-specific access.
  double getWhiteX() { return whiteX; }
  double getWhiteY() { return whiteY; }
  double getWhiteZ() { return whiteZ; }
  double getBlackX() { return blackX; }
  double getBlackY() { return blackY; }
  double getBlackZ() { return blackZ; }
  double getAMin() { return aMin; }
  double getAMax() { return aMax; }
  double getBMin() { return bMin; }
  double getBMax() { return bMax; }

private:

  double whiteX, whiteY, whiteZ;    // white point
  double blackX, blackY, blackZ;    // black point
  double aMin, aMax, bMin, bMax;    // range for the a and b components
  double kr, kg, kb;		    // gamut mapping mulitpliers
};

//------------------------------------------------------------------------
// GfxICCBasedColorSpace
//------------------------------------------------------------------------

class GfxICCBasedColorSpace: public GfxColorSpace {
public:

  GfxICCBasedColorSpace(int nComps, GfxColorSpace *alt,
			Ref *iccProfileStream);
  virtual ~GfxICCBasedColorSpace();
  virtual GfxColorSpace *copy();
  virtual GfxColorSpaceMode getMode() { return csICCBased; }

  // Construct an ICCBased color space.  Returns NULL if unsuccessful.
  static GfxColorSpace *parse(Array *arr);

  virtual void getGray(GfxColor *color, double *gray);
  virtual void getRGB(GfxColor *color, GfxRGB *rgb);
  virtual void getCMYK(GfxColor *color, GfxCMYK *cmyk);

  virtual int getNComps() { return nComps; }

  virtual void getDefaultRanges(double *decodeLow, double *decodeRange,
				int maxImgPixel);

  // ICCBased-specific access.
  GfxColorSpace *getAlt() { return alt; }

private:

  int nComps;			// number of color components (1, 3, or 4)
  GfxColorSpace *alt;		// alternate color space
  double rangeMin[4];		// min values for each component
  double rangeMax[4];		// max values for each component
  Ref iccProfileStream;		// the ICC profile
};

//------------------------------------------------------------------------
// GfxIndexedColorSpace
//------------------------------------------------------------------------

class GfxIndexedColorSpace: public GfxColorSpace {
public:

  GfxIndexedColorSpace(GfxColorSpace *base, int indexHigh);
  virtual ~GfxIndexedColorSpace();
  virtual GfxColorSpace *copy();
  virtual GfxColorSpaceMode getMode() { return csIndexed; }

  // Construct a Lab color space.  Returns NULL if unsuccessful.
  static GfxColorSpace *parse(Array *arr);

  virtual void getGray(GfxColor *color, double *gray);
  virtual void getRGB(GfxColor *color, GfxRGB *rgb);
  virtual void getCMYK(GfxColor *color, GfxCMYK *cmyk);

  virtual int getNComps() { return 1; }

  virtual void getDefaultRanges(double *decodeLow, double *decodeRange,
				int maxImgPixel);

  // Indexed-specific access.
  GfxColorSpace *getBase() { return base; }
  int getIndexHigh() { return indexHigh; }
  Guchar *getLookup() { return lookup; }

private:

  GfxColorSpace *base;		// base color space
  int indexHigh;		// max pixel value
  Guchar *lookup;		// lookup table
};

//------------------------------------------------------------------------
// GfxSeparationColorSpace
//------------------------------------------------------------------------

class GfxSeparationColorSpace: public GfxColorSpace {
public:

  GfxSeparationColorSpace(GString *name, GfxColorSpace *alt,
			  Function *func);
  virtual ~GfxSeparationColorSpace();
  virtual GfxColorSpace *copy();
  virtual GfxColorSpaceMode getMode() { return csSeparation; }

  // Construct a Separation color space.  Returns NULL if unsuccessful.
  static GfxColorSpace *parse(Array *arr);

  virtual void getGray(GfxColor *color, double *gray);
  virtual void getRGB(GfxColor *color, GfxRGB *rgb);
  virtual void getCMYK(GfxColor *color, GfxCMYK *cmyk);

  virtual int getNComps() { return 1; }

  // Separation-specific access.
  GString *getName() { return name; }
  GfxColorSpace *getAlt() { return alt; }
  Function *getFunc() { return func; }

private:

  GString *name;		// colorant name
  GfxColorSpace *alt;		// alternate color space
  Function *func;		// tint transform (into alternate color space)
};

//------------------------------------------------------------------------
// GfxDeviceNColorSpace
//------------------------------------------------------------------------

class GfxDeviceNColorSpace: public GfxColorSpace {
public:

  GfxDeviceNColorSpace(int nComps, GfxColorSpace *alt, Function *func);
  virtual ~GfxDeviceNColorSpace();
  virtual GfxColorSpace *copy();
  virtual GfxColorSpaceMode getMode() { return csDeviceN; }

  // Construct a DeviceN color space.  Returns NULL if unsuccessful.
  static GfxColorSpace *parse(Array *arr);

  virtual void getGray(GfxColor *color, double *gray);
  virtual void getRGB(GfxColor *color, GfxRGB *rgb);
  virtual void getCMYK(GfxColor *color, GfxCMYK *cmyk);

  virtual int getNComps() { return nComps; }

  // DeviceN-specific access.
  GfxColorSpace *getAlt() { return alt; }

private:

  int nComps;			// number of components
  GString			// colorant names
    *names[gfxColorMaxComps];
  GfxColorSpace *alt;		// alternate color space
  Function *func;		// tint transform (into alternate color space)
  
};

//------------------------------------------------------------------------
// GfxPatternColorSpace
//------------------------------------------------------------------------

class GfxPatternColorSpace: public GfxColorSpace {
public:

  GfxPatternColorSpace(GfxColorSpace *under);
  virtual ~GfxPatternColorSpace();
  virtual GfxColorSpace *copy();
  virtual GfxColorSpaceMode getMode() { return csPattern; }

  // Construct a Pattern color space.  Returns NULL if unsuccessful.
  static GfxColorSpace *parse(Array *arr);

  virtual void getGray(GfxColor *color, double *gray);
  virtual void getRGB(GfxColor *color, GfxRGB *rgb);
  virtual void getCMYK(GfxColor *color, GfxCMYK *cmyk);

  virtual int getNComps() { return 0; }

  // Pattern-specific access.
  GfxColorSpace *getUnder() { return under; }

private:

  GfxColorSpace *under;		// underlying color space (for uncolored
				//   patterns)
};

//------------------------------------------------------------------------
// GfxPattern
//------------------------------------------------------------------------

class GfxPattern {
public:

  GfxPattern(int type);
  virtual ~GfxPattern();

  static GfxPattern *parse(Object *obj);

  virtual GfxPattern *copy() = 0;

  int getType() { return type; }

private:

  int type;
};

//------------------------------------------------------------------------
// GfxTilingPattern
//------------------------------------------------------------------------

class GfxTilingPattern: public GfxPattern {
public:

  GfxTilingPattern(Dict *streamDict, Object *stream);
  virtual ~GfxTilingPattern();

  virtual GfxPattern *copy();

  int getPaintType() { return paintType; }
  int getTilingType() { return tilingType; }
  double *getBBox() { return bbox; }
  double getXStep() { return xStep; }
  double getYStep() { return yStep; }
  Dict *getResDict()
    { return resDict.isDict() ? resDict.getDict() : (Dict *)NULL; }
  double *getMatrix() { return matrix; }
  Object *getContentStream() { return &contentStream; }

private:

  GfxTilingPattern(GfxTilingPattern *pat);

  int paintType;
  int tilingType;
  double bbox[4];
  double xStep, yStep;
  Object resDict;
  double matrix[6];
  Object contentStream;
};

//------------------------------------------------------------------------
// Function
//------------------------------------------------------------------------

#define funcMaxInputs  1
#define funcMaxOutputs 8

class Function {
public:

  Function();

  virtual ~Function();

  // Construct a function.  Returns NULL if unsuccessful.
  static Function *parse(Object *funcObj);

  // Initialize the entries common to all function types.
  GBool init(Dict *dict);

  virtual Function *copy() = 0;

  // Return size of input and output tuples.
  int getInputSize() { return m; }
  int getOutputSize() { return n; }

  // Transform an input tuple into an output tuple.
  virtual void transform(double *in, double *out) = 0;

  virtual GBool isOk() = 0;

protected:

  int m, n;			// size of input and output tuples
  double			// min and max values for function domain
    domain[funcMaxInputs][2];
  double			// min and max values for function range
    range[funcMaxOutputs][2];
  GBool hasRange;		// set if range is defined
};

//------------------------------------------------------------------------
// SampledFunction
//------------------------------------------------------------------------

class SampledFunction: public Function {
public:

  SampledFunction(Object *funcObj, Dict *dict);
  virtual ~SampledFunction();
  virtual Function *copy() { return new SampledFunction(this); }
  virtual void transform(double *in, double *out);
  virtual GBool isOk() { return ok; }

private:

  SampledFunction(SampledFunction *func);

  int				// number of samples for each domain element
    sampleSize[funcMaxInputs];
  double			// min and max values for domain encoder
    encode[funcMaxInputs][2];
  double			// min and max values for range decoder
    decode[funcMaxOutputs][2];
  double *samples;		// the samples
  GBool ok;
};

//------------------------------------------------------------------------
// ExponentialFunction
//------------------------------------------------------------------------

class ExponentialFunction: public Function {
public:

  ExponentialFunction(Object *funcObj, Dict *dict);
  virtual ~ExponentialFunction();
  virtual Function *copy() { return new ExponentialFunction(this); }
  virtual void transform(double *in, double *out);
  virtual GBool isOk() { return ok; }

private:

  ExponentialFunction(ExponentialFunction *func);

  double c0[funcMaxOutputs];
  double c1[funcMaxOutputs];
  double e;
  GBool ok;
};

//------------------------------------------------------------------------
// GfxImageColorMap
//------------------------------------------------------------------------

class GfxImageColorMap {
public:

  // Constructor.
  GfxImageColorMap(int bits, Object *decode, GfxColorSpace *colorSpace);

  // Destructor.
  ~GfxImageColorMap();

  // Is color map valid?
  GBool isOk() { return ok; }

  // Get the color space.
  GfxColorSpace *getColorSpace() { return colorSpace; }

  // Get stream decoding info.
  int getNumPixelComps() { return nComps; }
  int getBits() { return bits; }

  // Get decode table.
  double getDecodeLow(int i) { return decodeLow[i]; }
  double getDecodeHigh(int i) { return decodeLow[i] + decodeRange[i]; }

  // Convert an image pixel to a color.
  void getGray(Guchar *x, double *gray);
  void getRGB(Guchar *x, GfxRGB *rgb);
  void getCMYK(Guchar *x, GfxCMYK *cmyk);

private:

  GfxColorSpace *colorSpace;	// the image color space
  int bits;			// bits per component
  int nComps;			// number of components in a pixel
  GfxColorSpace *colorSpace2;	// secondary color space
  int nComps2;			// number of components in colorSpace2
  double *lookup;		// lookup table
  double			// minimum values for each component
    decodeLow[gfxColorMaxComps];
  double			// max - min value for each component
    decodeRange[gfxColorMaxComps];
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
  GfxState(double dpi, double px1a, double py1a,
	   double px2a, double py2a, int rotate, GBool upsideDown);

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
  void getFillRGB(GfxRGB *rgb)
    { fillColorSpace->getRGB(&fillColor, rgb); }
  void getStrokeRGB(GfxRGB *rgb)
    { strokeColorSpace->getRGB(&strokeColor, rgb); }
  void getFillCMYK(GfxCMYK *cmyk)
    { fillColorSpace->getCMYK(&fillColor, cmyk); }
  void getStrokeCMYK(GfxCMYK *cmyk)
    { strokeColorSpace->getCMYK(&strokeColor, cmyk); }
  GfxColorSpace *getFillColorSpace() { return fillColorSpace; }
  GfxColorSpace *getStrokeColorSpace() { return strokeColorSpace; }
  GfxPattern *getFillPattern() { return fillPattern; }
  GfxPattern *getStrokePattern() { return strokePattern; }
  double getFillOpacity() { return fillOpacity; }
  double getStrokeOpacity() { return strokeOpacity; }
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
  void setFillColorSpace(GfxColorSpace *colorSpace);
  void setStrokeColorSpace(GfxColorSpace *colorSpace);
  void setFillColor(GfxColor *color) { fillColor = *color; }
  void setStrokeColor(GfxColor *color) { strokeColor = *color; }
  void setFillPattern(GfxPattern *pattern);
  void setStrokePattern(GfxPattern *pattern);
  void setFillOpacity(double opac) { fillOpacity = opac; }
  void setStrokeOpacity(double opac) { strokeOpacity = opac; }
  void setLineWidth(double width) { lineWidth = width; }
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
  GfxPattern *fillPattern;	// fill pattern
  GfxPattern *strokePattern;	// stroke pattern
  double fillOpacity;		// fill opacity
  double strokeOpacity;		// stroke opacity

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
