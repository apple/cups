//========================================================================
//
// Gfx.h
//
// Copyright 1996-2002 Glyph & Cog, LLC
//
//========================================================================

#ifndef GFX_H
#define GFX_H

#ifdef __GNUC__
#pragma interface
#endif

#include "gtypes.h"

class GString;
class XRef;
class Array;
class Stream;
class Parser;
class Dict;
class OutputDev;
class GfxFontDict;
class GfxFont;
class GfxPattern;
class GfxShading;
class GfxAxialShading;
class GfxRadialShading;
class GfxState;
class Gfx;
struct PDFRectangle;

//------------------------------------------------------------------------
// Gfx
//------------------------------------------------------------------------

enum GfxClipType {
  clipNone,
  clipNormal,
  clipEO
};

enum TchkType {
  tchkBool,			// boolean
  tchkInt,			// integer
  tchkNum,			// number (integer or real)
  tchkString,			// string
  tchkName,			// name
  tchkArray,			// array
  tchkProps,			// properties (dictionary or name)
  tchkSCN,			// scn/SCN args (number of name)
  tchkNone			// used to avoid empty initializer lists
};

#define maxArgs 8

struct Operator {
  char name[4];
  int numArgs;
  TchkType tchk[maxArgs];
  void (Gfx::*func)(Object args[], int numArgs);
};

class GfxResources {
public:

  GfxResources(XRef *xref, Dict *resDict, GfxResources *nextA);
  ~GfxResources();

  GfxFont *lookupFont(const char *name);
  GBool lookupXObject(const char *name, Object *obj);
  GBool lookupXObjectNF(const char *name, Object *obj);
  void lookupColorSpace(const char *name, Object *obj);
  GfxPattern *lookupPattern(const char *name);
  GfxShading *lookupShading(const char *name);
  GBool lookupGState(const char *name, Object *obj);

  GfxResources *getNext() { return next; }

private:

  GfxFontDict *fonts;
  Object xObjDict;
  Object colorSpaceDict;
  Object patternDict;
  Object shadingDict;
  Object gStateDict;
  GfxResources *next;
};

class Gfx {
public:

  // Constructor for regular output.
  Gfx(XRef *xrefA, OutputDev *outA, int pageNum, Dict *resDict, double dpi,
      PDFRectangle *box, GBool crop, PDFRectangle *cropBox, int rotate,
      GBool printCommandsA);

  // Constructor for a sub-page object.
  Gfx(XRef *xrefA, OutputDev *outA, Dict *resDict,
      PDFRectangle *box, GBool crop, PDFRectangle *cropBox);

  ~Gfx();

  // Interpret a stream or array of streams.
  void display(Object *obj, GBool topLevel = gTrue);

  // Display an annotation, given its appearance (a Form XObject) and
  // bounding box (in default user space).
  void doAnnot(Object *str, double xMin, double yMin,
	       double xMax, double yMax);

  void pushResources(Dict *resDict);
  void popResources();

private:

  XRef *xref;			// the xref table for this PDF file
  OutputDev *out;		// output device
  GBool subPage;		// is this a sub-page object?
  GBool printCommands;		// print the drawing commands (for debugging)
  GfxResources *res;		// resource stack
  int updateLevel;

  GfxState *state;		// current graphics state
  GBool fontChanged;		// set if font or text matrix has changed
  GfxClipType clip;		// do a clip?
  int ignoreUndef;		// current BX/EX nesting level
  double baseMatrix[6];		// default matrix for most recent
				//   page/form/pattern

  Parser *parser;		// parser for page content stream(s)

  static Operator opTab[];	// table of operators

  void go(GBool topLevel);
  void execOp(Object *cmd, Object args[], int numArgs);
  Operator *findOp(const char *name);
  GBool checkArg(Object *arg, TchkType type);
  int getPos();

  // graphics state operators
  void opSave(Object args[], int numArgs);
  void opRestore(Object args[], int numArgs);
  void opConcat(Object args[], int numArgs);
  void opSetDash(Object args[], int numArgs);
  void opSetFlat(Object args[], int numArgs);
  void opSetLineJoin(Object args[], int numArgs);
  void opSetLineCap(Object args[], int numArgs);
  void opSetMiterLimit(Object args[], int numArgs);
  void opSetLineWidth(Object args[], int numArgs);
  void opSetExtGState(Object args[], int numArgs);
  void opSetRenderingIntent(Object args[], int numArgs);

  // color operators
  void opSetFillGray(Object args[], int numArgs);
  void opSetStrokeGray(Object args[], int numArgs);
  void opSetFillCMYKColor(Object args[], int numArgs);
  void opSetStrokeCMYKColor(Object args[], int numArgs);
  void opSetFillRGBColor(Object args[], int numArgs);
  void opSetStrokeRGBColor(Object args[], int numArgs);
  void opSetFillColorSpace(Object args[], int numArgs);
  void opSetStrokeColorSpace(Object args[], int numArgs);
  void opSetFillColor(Object args[], int numArgs);
  void opSetStrokeColor(Object args[], int numArgs);
  void opSetFillColorN(Object args[], int numArgs);
  void opSetStrokeColorN(Object args[], int numArgs);

  // path segment operators
  void opMoveTo(Object args[], int numArgs);
  void opLineTo(Object args[], int numArgs);
  void opCurveTo(Object args[], int numArgs);
  void opCurveTo1(Object args[], int numArgs);
  void opCurveTo2(Object args[], int numArgs);
  void opRectangle(Object args[], int numArgs);
  void opClosePath(Object args[], int numArgs);

  // path painting operators
  void opEndPath(Object args[], int numArgs);
  void opStroke(Object args[], int numArgs);
  void opCloseStroke(Object args[], int numArgs);
  void opFill(Object args[], int numArgs);
  void opEOFill(Object args[], int numArgs);
  void opFillStroke(Object args[], int numArgs);
  void opCloseFillStroke(Object args[], int numArgs);
  void opEOFillStroke(Object args[], int numArgs);
  void opCloseEOFillStroke(Object args[], int numArgs);
  void doPatternFill(GBool eoFill);
  void opShFill(Object args[], int numArgs);
  void doAxialShFill(GfxAxialShading *shading);
  void doRadialShFill(GfxRadialShading *shading);
  void doEndPath();

  // path clipping operators
  void opClip(Object args[], int numArgs);
  void opEOClip(Object args[], int numArgs);

  // text object operators
  void opBeginText(Object args[], int numArgs);
  void opEndText(Object args[], int numArgs);

  // text state operators
  void opSetCharSpacing(Object args[], int numArgs);
  void opSetFont(Object args[], int numArgs);
  void opSetTextLeading(Object args[], int numArgs);
  void opSetTextRender(Object args[], int numArgs);
  void opSetTextRise(Object args[], int numArgs);
  void opSetWordSpacing(Object args[], int numArgs);
  void opSetHorizScaling(Object args[], int numArgs);

  // text positioning operators
  void opTextMove(Object args[], int numArgs);
  void opTextMoveSet(Object args[], int numArgs);
  void opSetTextMatrix(Object args[], int numArgs);
  void opTextNextLine(Object args[], int numArgs);

  // text string operators
  void opShowText(Object args[], int numArgs);
  void opMoveShowText(Object args[], int numArgs);
  void opMoveSetShowText(Object args[], int numArgs);
  void opShowSpaceText(Object args[], int numArgs);
  void doShowText(GString *s);

  // XObject operators
  void opXObject(Object args[], int numArgs);
  void doImage(Object *ref, Stream *str, GBool inlineImg);
  void doForm(Object *str);
  void doForm1(Object *str, Dict *resDict, double *matrix, double *bbox);

  // in-line image operators
  void opBeginImage(Object args[], int numArgs);
  Stream *buildImageStream();
  void opImageData(Object args[], int numArgs);
  void opEndImage(Object args[], int numArgs);

  // type 3 font operators
  void opSetCharWidth(Object args[], int numArgs);
  void opSetCacheDevice(Object args[], int numArgs);

  // compatibility operators
  void opBeginIgnoreUndef(Object args[], int numArgs);
  void opEndIgnoreUndef(Object args[], int numArgs);

  // marked content operators
  void opBeginMarkedContent(Object args[], int numArgs);
  void opEndMarkedContent(Object args[], int numArgs);
  void opMarkPoint(Object args[], int numArgs);
};

#endif
