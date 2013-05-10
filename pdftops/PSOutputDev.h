//========================================================================
//
// PSOutputDev.h
//
// Copyright 1996-2002 Glyph & Cog, LLC
//
//========================================================================

#ifndef PSOUTPUTDEV_H
#define PSOUTPUTDEV_H

#include <config.h>

#ifdef USE_GCC_PRAGMAS
#pragma interface
#endif

#include <stddef.h>
#include "config.h"
#include "Object.h"
#include "GlobalParams.h"
#include "OutputDev.h"

class GfxPath;
class GfxFont;
class GfxColorSpace;
class GfxSeparationColorSpace;
struct PSFont16Enc;
class PSOutCustomColor;

//------------------------------------------------------------------------
// PSOutputDev
//------------------------------------------------------------------------

enum PSOutMode {
  psModePS,
  psModeEPS,
  psModeForm
};

enum PSFileType {
  psFile,			// write to file
  psPipe,			// write to pipe
  psStdout,			// write to stdout
  psGeneric			// write to a generic stream
};

typedef void (*PSOutputFunc)(void *stream, char *data, int len);

class PSOutputDev: public OutputDev {
public:

  // Open a PostScript output file, and write the prolog.
  PSOutputDev(char *fileName, XRef *xrefA, Catalog *catalog,
	      int firstPage, int lastPage, PSOutMode modeA);

  // Open a PSOutputDev that will write to a generic stream.
  PSOutputDev(PSOutputFunc outputFuncA, void *outputStreamA,
	      XRef *xrefA, Catalog *catalog,
	      int firstPage, int lastPage, PSOutMode modeA);

  // Destructor -- writes the trailer and closes the file.
  virtual ~PSOutputDev();

  // Check if file was successfully created.
  virtual GBool isOk() { return ok; }

  //---- get info about output device

  // Does this device use upside-down coordinates?
  // (Upside-down means (0,0) is the top left corner of the page.)
  virtual GBool upsideDown() { return gFalse; }

  // Does this device use drawChar() or drawString()?
  virtual GBool useDrawChar() { return gFalse; }

  // Does this device use beginType3Char/endType3Char?  Otherwise,
  // text in Type 3 fonts will be drawn with drawChar/drawString.
  virtual GBool interpretType3Chars() { return gFalse; }

  //----- initialization and control

  // Start a page.
  virtual void startPage(int pageNum, GfxState *state);

  // End a page.
  virtual void endPage();

  //----- save/restore graphics state
  virtual void saveState(GfxState *state);
  virtual void restoreState(GfxState *state);

  //----- update graphics state
  virtual void updateCTM(GfxState *state, double m11, double m12,
			 double m21, double m22, double m31, double m32);
  virtual void updateLineDash(GfxState *state);
  virtual void updateFlatness(GfxState *state);
  virtual void updateLineJoin(GfxState *state);
  virtual void updateLineCap(GfxState *state);
  virtual void updateMiterLimit(GfxState *state);
  virtual void updateLineWidth(GfxState *state);
  virtual void updateFillColor(GfxState *state);
  virtual void updateStrokeColor(GfxState *state);

  //----- update text state
  virtual void updateFont(GfxState *state);
  virtual void updateTextMat(GfxState *state);
  virtual void updateCharSpace(GfxState *state);
  virtual void updateRender(GfxState *state);
  virtual void updateRise(GfxState *state);
  virtual void updateWordSpace(GfxState *state);
  virtual void updateHorizScaling(GfxState *state);
  virtual void updateTextPos(GfxState *state);
  virtual void updateTextShift(GfxState *state, double shift);

  //----- path painting
  virtual void stroke(GfxState *state);
  virtual void fill(GfxState *state);
  virtual void eoFill(GfxState *state);

  //----- path clipping
  virtual void clip(GfxState *state);
  virtual void eoClip(GfxState *state);

  //----- text drawing
  virtual void drawString(GfxState *state, GString *s);

  //----- image drawing
  virtual void drawImageMask(GfxState *state, Object *ref, Stream *str,
			     int width, int height, GBool invert,
			     GBool inlineImg);
  virtual void drawImage(GfxState *state, Object *ref, Stream *str,
			 int width, int height, GfxImageColorMap *colorMap,
			 int *maskColors, GBool inlineImg);

#if OPI_SUPPORT
  //----- OPI functions
  virtual void opiBegin(GfxState *state, Dict *opiDict);
  virtual void opiEnd(GfxState *state, Dict *opiDict);
#endif

  //----- Type 3 font operators
  virtual void type3D0(GfxState *state, double wx, double wy);
  virtual void type3D1(GfxState *state, double wx, double wy,
		       double llx, double lly, double urx, double ury);

  //----- PostScript XObjects
  virtual void psXObject(Stream *psStream, Stream *level1Stream);


private:

  void init(PSOutputFunc outputFuncA, void *outputStreamA,
	    PSFileType fileTypeA, XRef *xrefA, Catalog *catalog,
	    int firstPage, int lastPage, PSOutMode modeA);
  void setupResources(Dict *resDict);
  void setupFonts(Dict *resDict);
  void setupFont(GfxFont *font, Dict *parentResDict);
  void setupEmbeddedType1Font(Ref *id, char *psName);
  void setupExternalType1Font(GString *fileName, char *psName);
  void setupEmbeddedType1CFont(GfxFont *font, Ref *id, char *psName);
  void setupEmbeddedTrueTypeFont(GfxFont *font, Ref *id, char *psName);
  void setupExternalTrueTypeFont(GfxFont *font, char *psName);
  void setupEmbeddedCIDType0Font(GfxFont *font, Ref *id, char *psName);
  void setupEmbeddedCIDTrueTypeFont(GfxFont *font, Ref *id, char *psName);
  void setupType3Font(GfxFont *font, char *psName, Dict *parentResDict);
  void setupImages(Dict *resDict);
  void setupImage(Ref id, Stream *str);
  void addProcessColor(double c, double m, double y, double k);
  void addCustomColor(GfxSeparationColorSpace *sepCS);
  void doPath(GfxPath *path);
  void doImageL1(GfxImageColorMap *colorMap,
		 GBool invert, GBool inlineImg,
		 Stream *str, int width, int height, int len);
  void doImageL1Sep(GfxImageColorMap *colorMap,
		    GBool invert, GBool inlineImg,
		    Stream *str, int width, int height, int len);
  void doImageL2(Object *ref, GfxImageColorMap *colorMap,
		 GBool invert, GBool inlineImg,
		 Stream *str, int width, int height, int len);
  void dumpColorSpaceL2(GfxColorSpace *colorSpace);
#if OPI_SUPPORT
  void opiBegin20(GfxState *state, Dict *dict);
  void opiBegin13(GfxState *state, Dict *dict);
  void opiTransform(GfxState *state, double x0, double y0,
		    double *x1, double *y1);
  GBool getFileSpec(Object *fileSpec, Object *fileName);
#endif
  void writePSChar(char c);
  void writePS(char *s);
  void writePSFmt(const char *fmt, ...);
  void writePSString(GString *s);
  void writePSName(char *s);
  GString *filterPSName(GString *name);

  PSLevel level;		// PostScript level (1, 2, separation)
  PSOutMode mode;		// PostScript mode (PS, EPS, form)
  int paperWidth;		// width of paper, in pts
  int paperHeight;		// height of paper, in pts

  PSOutputFunc outputFunc;
  void *outputStream;
  PSFileType fileType;		// file / pipe / stdout
  int seqPage;			// current sequential page number

  XRef *xref;			// the xref table for this PDF file

  Ref *fontIDs;			// list of object IDs of all used fonts
  int fontIDLen;		// number of entries in fontIDs array
  int fontIDSize;		// size of fontIDs array
  Ref *fontFileIDs;		// list of object IDs of all embedded fonts
  int fontFileIDLen;		// number of entries in fontFileIDs array
  int fontFileIDSize;		// size of fontFileIDs array
  GString **fontFileNames;	// list of names of all embedded external fonts
  int fontFileNameLen;		// number of entries in fontFileNames array
  int fontFileNameSize;		// size of fontFileNames array
  PSFont16Enc *font16Enc;	// encodings for substitute 16-bit fonts
  int font16EncLen;		// number of entries in font16Enc array
  int font16EncSize;		// size of font16Enc array

  double tx, ty;		// global translation
  double xScale, yScale;	// global scaling
  GBool landscape;		// true for landscape, false for portrait

  GString *embFontList;		// resource comments for embedded fonts

  int processColors;		// used process colors
  PSOutCustomColor		// used custom colors
    *customColors;

  GBool inType3Char;		// inside a Type 3 CharProc
  GString *t3String;		// Type 3 content string
  double t3WX, t3WY,		// Type 3 character parameters
         t3LLX, t3LLY, t3URX, t3URY;
  GBool t3Cacheable;		// cleared if char is not cacheable

#if OPI_SUPPORT
  int opi13Nest;		// nesting level of OPI 1.3 objects
  int opi20Nest;		// nesting level of OPI 2.0 objects
#endif

  GBool ok;			// set up ok?

};

#endif
