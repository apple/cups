//========================================================================
//
// PSOutputDev.h
//
// Copyright 1996 Derek B. Noonburg
//
//========================================================================

#ifndef PSOUTPUTDEV_H
#define PSOUTPUTDEV_H

#ifdef __GNUC__
#pragma interface
#endif

#include <stddef.h>
#include "config.h"
#include "Object.h"
#include "OutputDev.h"

class GfxPath;
class GfxFont;
class GfxColorSpace;

//------------------------------------------------------------------------
// Parameters
//------------------------------------------------------------------------

// Generate Level 1 PostScript?
extern GBool psOutLevel1;

// Generate Level 1 separable PostScript?
extern GBool psOutLevel1Sep;

// Generate Encapsulated PostScript?
extern GBool psOutEPS;

#if OPI_SUPPORT
// Generate OPI comments?
extern GBool psOutOPI;
#endif

// Paper size.
extern int paperWidth;
extern int paperHeight;

//------------------------------------------------------------------------
// PSOutputDev
//------------------------------------------------------------------------

enum PSFileType {
  psFile,			// write to file
  psPipe,			// write to pipe
  psStdout			// write to stdout
};

class PSOutputDev: public OutputDev {
public:

  // Open a PostScript output file, and write the prolog.
  PSOutputDev(const char *fileName, Catalog *catalog,
	      int firstPage, int lastPage,
	      GBool embedType11, GBool doForm1);

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
  virtual void drawString16(GfxState *state, GString *s);

  //----- image drawing
  virtual void drawImageMask(GfxState *state, Object *ref, Stream *str,
			     int width, int height, GBool invert,
			     GBool inlineImg);
  virtual void drawImage(GfxState *state, Object *ref, Stream *str,
			 int width, int height, GfxImageColorMap *colorMap,
			 GBool inlineImg);

#if OPI_SUPPORT
  //----- OPI functions
  virtual void opiBegin(GfxState *state, Dict *opiDict);
  virtual void opiEnd(GfxState *state, Dict *opiDict);
#endif

private:

  void setupResources(Dict *resDict);
  void setupFonts(Dict *resDict);
  void setupFont(GfxFont *font);
  void setupEmbeddedType1Font(Ref *id, const char *psName);
  void setupEmbeddedType1Font(GString *fileName, const char *psName);
  void setupEmbeddedType1CFont(GfxFont *font, Ref *id, const char *psName);
  void setupImages(Dict *resDict);
  void setupImage(Ref id, Stream *str);
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
  void opiBegin20(GfxState *state, Dict *dict);
  void opiBegin13(GfxState *state, Dict *dict);
  void opiTransform(GfxState *state, double x0, double y0,
		    double *x1, double *y1);
  GBool getFileSpec(Object *fileSpec, Object *fileName);
  void writePS(const char *fmt, ...);
  void writePSString(GString *s);

  GBool embedType1;		// embed Type 1 fonts?
  GBool doForm;			// generate a form?

  FILE *f;			// PostScript file
  PSFileType fileType;		// file / pipe / stdout
  int seqPage;			// current sequential page number

  Ref *fontIDs;			// list of object IDs of all used fonts
  int fontIDLen;		// number of entries in fontIDs array
  int fontIDSize;		// size of fontIDs array
  Ref *fontFileIDs;		// list of object IDs of all embedded fonts
  int fontFileIDLen;		// number of entries in fontFileIDs array
  int fontFileIDSize;		// size of fontFileIDs array
  GString **fontFileNames;	// list of names of all embedded external fonts
  int fontFileNameLen;		// number of entries in fontFileNames array
  int fontFileNameSize;		// size of fontFileNames array

  double tx, ty;		// global translation
  double xScale, yScale;	// global scaling
  GBool landscape;		// true for landscape, false for portrait

  GString *embFontList;		// resource comments for embedded fonts

#if OPI_SUPPORT
  int opi13Nest;		// nesting level of OPI 1.3 objects
  int opi20Nest;		// nesting level of OPI 2.0 objects
#endif

  GBool type3Warning;		// only show the Type 3 font warning once

  GBool ok;			// set up ok?
};

#endif
