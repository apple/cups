//========================================================================
//
// Gfx.h
//
// Copyright 1996 Derek B. Noonburg
//
//========================================================================

#ifndef GFX_H
#define GFX_H

#ifdef __GNUC__
#pragma interface
#endif

#include "gtypes.h"

class GString;
class Array;
class Stream;
class Parser;
class Dict;
class OutputDev;
class GfxFontDict;
class GfxFont;
struct GfxFontEncoding16;
class GfxState;
class Gfx;

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

  GfxResources(GfxResources *next1) { fonts = NULL; next = next1; }
  ~GfxResources();

  GfxFontDict *fonts;
  Object xObjDict;
  Object colorSpaceDict;
  GfxResources *next;
};

class Gfx {
public:

  // Constructor for regular output.
  Gfx(OutputDev *out1, int pageNum, Dict *resDict,
      int dpi, double x1, double y1, double x2, double y2, GBool crop,
      double cropX1, double cropY1, double cropX2, double cropY2,
      int rotate);

  // Destructor.
  ~Gfx();

  // Interpret a stream or array of streams.
  void display(Object *obj);

private:

  OutputDev *out;		// output device
  GfxResources *res;		// resource stack

  GfxState *state;		// current graphics state
  GBool fontChanged;		// set if font or text matrix has changed
  GfxClipType clip;		// do a clip?
  int ignoreUndef;		// current BX/EX nesting level

  Parser *parser;		// parser for page content stream(s)

  static Operator opTab[];	// table of operators

  void go();
  void execOp(Object *cmd, Object args[], int numArgs);
  Operator *findOp(char *name);
  GBool checkArg(Object *arg, TchkType type);
  int getPos();
  GfxFont *lookupFont(char *name);
  GBool lookupXObject(char *name, Object *obj);
  void lookupColorSpace(char *name, Object *obj);

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
  void opShFill(Object args[], int numArgs);
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
  int getNextChar16(GfxFontEncoding16 *enc, Guchar *p, int *c16);

  // XObject operators
  void opXObject(Object args[], int numArgs);
  void doImage(Stream *str, GBool inlineImg);
  void doForm(Object *str);

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
