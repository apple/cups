//========================================================================
//
// Gfx.cc
//
// Copyright 1996 Derek B. Noonburg
//
//========================================================================

#ifdef __GNUC__
#pragma implementation
#endif

#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <math.h>
#include "gmem.h"
#include "Object.h"
#include "Array.h"
#include "Dict.h"
#include "Stream.h"
#include "Lexer.h"
#include "Parser.h"
#include "GfxFont.h"
#include "GfxState.h"
#include "OutputDev.h"
#include "Params.h"
#include "Page.h"
#include "Error.h"
#include "Gfx.h"

//------------------------------------------------------------------------
// constants
//------------------------------------------------------------------------

// Max number of splits along the t axis for an axial shading fill.
#define axialMaxSplits 256

// Max delta allowed in any color component for an axial shading fill.
#define axialColorDelta (1 / 256.0)

// Some systems don't define the constant for PI...
#ifndef M_PI
#  define M_PI		3.14159265358979323846
#endif // !M_PI

//------------------------------------------------------------------------
// Operator table
//------------------------------------------------------------------------

Operator Gfx::opTab[] = {
  {"\"",  3, {tchkNum,    tchkNum,    tchkString},
          &Gfx::opMoveSetShowText},
  {"'",   1, {tchkString},
          &Gfx::opMoveShowText},
  {"B",   0, {tchkNone},
          &Gfx::opFillStroke},
  {"B*",  0, {tchkNone},
          &Gfx::opEOFillStroke},
  {"BDC", 2, {tchkName,   tchkProps},
          &Gfx::opBeginMarkedContent},
  {"BI",  0, {tchkNone},
          &Gfx::opBeginImage},
  {"BMC", 1, {tchkName},
          &Gfx::opBeginMarkedContent},
  {"BT",  0, {tchkNone},
          &Gfx::opBeginText},
  {"BX",  0, {tchkNone},
          &Gfx::opBeginIgnoreUndef},
  {"CS",  1, {tchkName},
          &Gfx::opSetStrokeColorSpace},
  {"DP",  2, {tchkName,   tchkProps},
          &Gfx::opMarkPoint},
  {"Do",  1, {tchkName},
          &Gfx::opXObject},
  {"EI",  0, {tchkNone},
          &Gfx::opEndImage},
  {"EMC", 0, {tchkNone},
          &Gfx::opEndMarkedContent},
  {"ET",  0, {tchkNone},
          &Gfx::opEndText},
  {"EX",  0, {tchkNone},
          &Gfx::opEndIgnoreUndef},
  {"F",   0, {tchkNone},
          &Gfx::opFill},
  {"G",   1, {tchkNum},
          &Gfx::opSetStrokeGray},
  {"ID",  0, {tchkNone},
          &Gfx::opImageData},
  {"J",   1, {tchkInt},
          &Gfx::opSetLineCap},
  {"K",   4, {tchkNum,    tchkNum,    tchkNum,    tchkNum},
          &Gfx::opSetStrokeCMYKColor},
  {"M",   1, {tchkNum},
          &Gfx::opSetMiterLimit},
  {"MP",  1, {tchkName},
          &Gfx::opMarkPoint},
  {"Q",   0, {tchkNone},
          &Gfx::opRestore},
  {"RG",  3, {tchkNum,    tchkNum,    tchkNum},
          &Gfx::opSetStrokeRGBColor},
  {"S",   0, {tchkNone},
          &Gfx::opStroke},
  {"SC",  -4, {tchkNum,   tchkNum,    tchkNum,    tchkNum},
          &Gfx::opSetStrokeColor},
  {"SCN", -5, {tchkSCN,   tchkSCN,    tchkSCN,    tchkSCN,
	       tchkSCN},
          &Gfx::opSetStrokeColorN},
  {"T*",  0, {tchkNone},
          &Gfx::opTextNextLine},
  {"TD",  2, {tchkNum,    tchkNum},
          &Gfx::opTextMoveSet},
  {"TJ",  1, {tchkArray},
          &Gfx::opShowSpaceText},
  {"TL",  1, {tchkNum},
          &Gfx::opSetTextLeading},
  {"Tc",  1, {tchkNum},
          &Gfx::opSetCharSpacing},
  {"Td",  2, {tchkNum,    tchkNum},
          &Gfx::opTextMove},
  {"Tf",  2, {tchkName,   tchkNum},
          &Gfx::opSetFont},
  {"Tj",  1, {tchkString},
          &Gfx::opShowText},
  {"Tm",  6, {tchkNum,    tchkNum,    tchkNum,    tchkNum,
	      tchkNum,    tchkNum},
          &Gfx::opSetTextMatrix},
  {"Tr",  1, {tchkInt},
          &Gfx::opSetTextRender},
  {"Ts",  1, {tchkNum},
          &Gfx::opSetTextRise},
  {"Tw",  1, {tchkNum},
          &Gfx::opSetWordSpacing},
  {"Tz",  1, {tchkNum},
          &Gfx::opSetHorizScaling},
  {"W",   0, {tchkNone},
          &Gfx::opClip},
  {"W*",  0, {tchkNone},
          &Gfx::opEOClip},
  {"b",   0, {tchkNone},
          &Gfx::opCloseFillStroke},
  {"b*",  0, {tchkNone},
          &Gfx::opCloseEOFillStroke},
  {"c",   6, {tchkNum,    tchkNum,    tchkNum,    tchkNum,
	      tchkNum,    tchkNum},
          &Gfx::opCurveTo},
  {"cm",  6, {tchkNum,    tchkNum,    tchkNum,    tchkNum,
	      tchkNum,    tchkNum},
          &Gfx::opConcat},
  {"cs",  1, {tchkName},
          &Gfx::opSetFillColorSpace},
  {"d",   2, {tchkArray,  tchkNum},
          &Gfx::opSetDash},
  {"d0",  2, {tchkNum,    tchkNum},
          &Gfx::opSetCharWidth},
  {"d1",  6, {tchkNum,    tchkNum,    tchkNum,    tchkNum,
	      tchkNum,    tchkNum},
          &Gfx::opSetCacheDevice},
  {"f",   0, {tchkNone},
          &Gfx::opFill},
  {"f*",  0, {tchkNone},
          &Gfx::opEOFill},
  {"g",   1, {tchkNum},
          &Gfx::opSetFillGray},
  {"gs",  1, {tchkName},
          &Gfx::opSetExtGState},
  {"h",   0, {tchkNone},
          &Gfx::opClosePath},
  {"i",   1, {tchkNum},
          &Gfx::opSetFlat},
  {"j",   1, {tchkInt},
          &Gfx::opSetLineJoin},
  {"k",   4, {tchkNum,    tchkNum,    tchkNum,    tchkNum},
          &Gfx::opSetFillCMYKColor},
  {"l",   2, {tchkNum,    tchkNum},
          &Gfx::opLineTo},
  {"m",   2, {tchkNum,    tchkNum},
          &Gfx::opMoveTo},
  {"n",   0, {tchkNone},
          &Gfx::opEndPath},
  {"q",   0, {tchkNone},
          &Gfx::opSave},
  {"re",  4, {tchkNum,    tchkNum,    tchkNum,    tchkNum},
          &Gfx::opRectangle},
  {"rg",  3, {tchkNum,    tchkNum,    tchkNum},
          &Gfx::opSetFillRGBColor},
  {"ri",  1, {tchkName},
          &Gfx::opSetRenderingIntent},
  {"s",   0, {tchkNone},
          &Gfx::opCloseStroke},
  {"sc",  -4, {tchkNum,   tchkNum,    tchkNum,    tchkNum},
          &Gfx::opSetFillColor},
  {"scn", -5, {tchkSCN,   tchkSCN,    tchkSCN,    tchkSCN,
	       tchkSCN},
          &Gfx::opSetFillColorN},
  {"sh",  1, {tchkName},
          &Gfx::opShFill},
  {"v",   4, {tchkNum,    tchkNum,    tchkNum,    tchkNum},
          &Gfx::opCurveTo1},
  {"w",   1, {tchkNum},
          &Gfx::opSetLineWidth},
  {"y",   4, {tchkNum,    tchkNum,    tchkNum,    tchkNum},
          &Gfx::opCurveTo2},
};

#define numOps (sizeof(opTab) / sizeof(Operator))

//------------------------------------------------------------------------
// GfxResources
//------------------------------------------------------------------------

GfxResources::GfxResources(XRef *xref, Dict *resDict, GfxResources *nextA) {
  Object obj1;

  if (resDict) {

    // build font dictionary
    fonts = NULL;
    resDict->lookup("Font", &obj1);
    if (obj1.isDict()) {
      fonts = new GfxFontDict(xref, obj1.getDict());
    }
    obj1.free();

    // get XObject dictionary
    resDict->lookup("XObject", &xObjDict);

    // get color space dictionary
    resDict->lookup("ColorSpace", &colorSpaceDict);

    // get pattern dictionary
    resDict->lookup("Pattern", &patternDict);

    // get shading dictionary
    resDict->lookup("Shading", &shadingDict);

    // get graphics state parameter dictionary
    resDict->lookup("ExtGState", &gStateDict);

  } else {
    fonts = NULL;
    xObjDict.initNull();
    colorSpaceDict.initNull();
    patternDict.initNull();
    gStateDict.initNull();
  }

  next = nextA;
}

GfxResources::~GfxResources() {
  if (fonts) {
    delete fonts;
  }
  xObjDict.free();
  colorSpaceDict.free();
  patternDict.free();
  shadingDict.free();
  gStateDict.free();
}

GfxFont *GfxResources::lookupFont(char *name) {
  GfxFont *font;
  GfxResources *resPtr;

  for (resPtr = this; resPtr; resPtr = resPtr->next) {
    if (resPtr->fonts) {
      if ((font = resPtr->fonts->lookup(name)))
	return font;
    }
  }
  error(-1, "Unknown font tag '%s'", name);
  return NULL;
}

GBool GfxResources::lookupXObject(char *name, Object *obj) {
  GfxResources *resPtr;

  for (resPtr = this; resPtr; resPtr = resPtr->next) {
    if (resPtr->xObjDict.isDict()) {
      if (!resPtr->xObjDict.dictLookup(name, obj)->isNull())
	return gTrue;
      obj->free();
    }
  }
  error(-1, "XObject '%s' is unknown", name);
  return gFalse;
}

GBool GfxResources::lookupXObjectNF(char *name, Object *obj) {
  GfxResources *resPtr;

  for (resPtr = this; resPtr; resPtr = resPtr->next) {
    if (resPtr->xObjDict.isDict()) {
      if (!resPtr->xObjDict.dictLookupNF(name, obj)->isNull())
	return gTrue;
      obj->free();
    }
  }
  error(-1, "XObject '%s' is unknown", name);
  return gFalse;
}

void GfxResources::lookupColorSpace(char *name, Object *obj) {
  GfxResources *resPtr;

  for (resPtr = this; resPtr; resPtr = resPtr->next) {
    if (resPtr->colorSpaceDict.isDict()) {
      if (!resPtr->colorSpaceDict.dictLookup(name, obj)->isNull()) {
	return;
      }
      obj->free();
    }
  }
  obj->initNull();
}

GfxPattern *GfxResources::lookupPattern(char *name) {
  GfxResources *resPtr;
  GfxPattern *pattern;
  Object obj;

  for (resPtr = this; resPtr; resPtr = resPtr->next) {
    if (resPtr->patternDict.isDict()) {
      if (!resPtr->patternDict.dictLookup(name, &obj)->isNull()) {
	pattern = GfxPattern::parse(&obj);
	obj.free();
	return pattern;
      }
      obj.free();
    }
  }
  error(-1, "Unknown pattern '%s'", name);
  return NULL;
}

GfxShading *GfxResources::lookupShading(char *name) {
  GfxResources *resPtr;
  GfxShading *shading;
  Object obj;

  for (resPtr = this; resPtr; resPtr = resPtr->next) {
    if (resPtr->shadingDict.isDict()) {
      if (!resPtr->shadingDict.dictLookup(name, &obj)->isNull()) {
	shading = GfxShading::parse(&obj);
	obj.free();
	return shading;
      }
      obj.free();
    }
  }
  error(-1, "Unknown shading '%s'", name);
  return NULL;
}

GBool GfxResources::lookupGState(char *name, Object *obj) {
  GfxResources *resPtr;

  for (resPtr = this; resPtr; resPtr = resPtr->next) {
    if (resPtr->gStateDict.isDict()) {
      if (!resPtr->gStateDict.dictLookup(name, obj)->isNull()) {
	return gTrue;
      }
      obj->free();
    }
  }
  error(-1, "ExtGState '%s' is unknown", name);
  return gFalse;
}

//------------------------------------------------------------------------
// Gfx
//------------------------------------------------------------------------

Gfx::Gfx(XRef *xrefA, OutputDev *outA, int pageNum, Dict *resDict, double dpi,
	 PDFRectangle *box, GBool crop, PDFRectangle *cropBox, int rotate,
	 GBool printCommandsA) {
  int i;

  xref = xrefA;
  printCommands = printCommandsA;

  // start the resource stack
  res = new GfxResources(xref, resDict, NULL);

  // initialize
  out = outA;
  state = new GfxState(dpi, box, rotate, out->upsideDown());
  fontChanged = gFalse;
  clip = clipNone;
  ignoreUndef = 0;
  out->startPage(pageNum, state);
  out->setDefaultCTM(state->getCTM());
  out->updateAll(state);
  for (i = 0; i < 6; ++i) {
    baseMatrix[i] = state->getCTM()[i];
  }

  // set crop box
  if (crop) {
    state->moveTo(cropBox->x1, cropBox->y1);
    state->lineTo(cropBox->x2, cropBox->y1);
    state->lineTo(cropBox->x2, cropBox->y2);
    state->lineTo(cropBox->x1, cropBox->y2);
    state->closePath();
    state->clip();
    out->clip(state);
    state->clearPath();
  }
}

Gfx::~Gfx() {
  GfxResources *resPtr;

  while (state->hasSaves()) {
    state = state->restore();
    out->restoreState(state);
  }
  out->endPage();
  while (res) {
    resPtr = res->getNext();
    delete res;
    res = resPtr;
  }
  if (state)
    delete state;
}

void Gfx::display(Object *obj, GBool topLevel) {
  Object obj2;
  int i;

  if (obj->isArray()) {
    for (i = 0; i < obj->arrayGetLength(); ++i) {
      obj->arrayGet(i, &obj2);
      if (!obj2.isStream()) {
	error(-1, "Weird page contents");
	obj2.free();
	return;
      }
      obj2.free();
    }
  } else if (!obj->isStream()) {
    error(-1, "Weird page contents");
    return;
  }
  parser = new Parser(xref, new Lexer(xref, obj));
  go(topLevel);
  delete parser;
  parser = NULL;
}

void Gfx::go(GBool topLevel) {
  Object obj;
  Object args[maxArgs];
  int numCmds, numArgs;
  int i;

  // scan a sequence of objects
  numCmds = 0;
  numArgs = 0;
  parser->getObj(&obj);
  while (!obj.isEOF()) {

    // got a command - execute it
    if (obj.isCmd()) {
      if (printCommands) {
	obj.print(stdout);
	for (i = 0; i < numArgs; ++i) {
	  printf(" ");
	  args[i].print(stdout);
	}
	printf("\n");
	fflush(stdout);
      }
      execOp(&obj, args, numArgs);
      obj.free();
      for (i = 0; i < numArgs; ++i)
	args[i].free();
      numArgs = 0;

      // periodically update display
      if (++numCmds == 200) {
	out->dump();
	numCmds = 0;
      }

    // got an argument - save it
    } else if (numArgs < maxArgs) {
      args[numArgs++] = obj;

    // too many arguments - something is wrong
    } else {
      error(getPos(), "Too many args in content stream");
      if (printCommands) {
	printf("throwing away arg: ");
	obj.print(stdout);
	printf("\n");
	fflush(stdout);
      }
      obj.free();
    }

    // grab the next object
    parser->getObj(&obj);
  }
  obj.free();

  // args at end with no command
  if (numArgs > 0) {
    error(getPos(), "Leftover args in content stream");
    if (printCommands) {
      printf("%d leftovers:", numArgs);
      for (i = 0; i < numArgs; ++i) {
	printf(" ");
	args[i].print(stdout);
      }
      printf("\n");
      fflush(stdout);
    }
    for (i = 0; i < numArgs; ++i)
      args[i].free();
  }

  // update display
  if (topLevel && numCmds > 0) {
    out->dump();
  }
}

void Gfx::execOp(Object *cmd, Object args[], int numArgs) {
  Operator *op;
  char *name;
  int i;

  // find operator
  name = cmd->getName();
  if (!(op = findOp(name))) {
    if (ignoreUndef == 0)
      error(getPos(), "Unknown operator '%s'", name);
    return;
  }

  // type check args
  if (op->numArgs >= 0) {
    if (numArgs != op->numArgs) {
      error(getPos(), "Wrong number (%d) of args to '%s' operator",
	    numArgs, name);
      return;
    }
  } else {
    if (numArgs > -op->numArgs) {
      error(getPos(), "Too many (%d) args to '%s' operator",
	    numArgs, name);
      return;
    }
  }
  for (i = 0; i < numArgs; ++i) {
    if (!checkArg(&args[i], op->tchk[i])) {
      error(getPos(), "Arg #%d to '%s' operator is wrong type (%s)",
	    i, name, args[i].getTypeName());
      return;
    }
  }

  // do it
  (this->*op->func)(args, numArgs);
}

Operator *Gfx::findOp(char *name) {
  int a, b, m, cmp;

  a = -1;
  b = numOps;
  // invariant: opTab[a] < name < opTab[b]
  while (b - a > 1) {
    m = (a + b) / 2;
    cmp = strcmp(opTab[m].name, name);
    if (cmp < 0)
      a = m;
    else if (cmp > 0)
      b = m;
    else
      a = b = m;
  }
  if (cmp != 0)
    return NULL;
  return &opTab[a];
}

GBool Gfx::checkArg(Object *arg, TchkType type) {
  switch (type) {
  case tchkBool:   return arg->isBool();
  case tchkInt:    return arg->isInt();
  case tchkNum:    return arg->isNum();
  case tchkString: return arg->isString();
  case tchkName:   return arg->isName();
  case tchkArray:  return arg->isArray();
  case tchkProps:  return arg->isDict() || arg->isName();
  case tchkSCN:    return arg->isNum() || arg->isName();
  case tchkNone:   return gFalse;
  }
  return gFalse;
}

int Gfx::getPos() {
  return parser ? parser->getPos() : -1;
}

//------------------------------------------------------------------------
// graphics state operators
//------------------------------------------------------------------------

void Gfx::opSave(Object args[], int numArgs) {
  out->saveState(state);
  state = state->save();
}

void Gfx::opRestore(Object args[], int numArgs) {
  state = state->restore();
  out->restoreState(state);

  // Some PDF producers (Macromedia FreeHand) generate a save (q) and
  // restore (Q) inside a path sequence.  The PDF spec seems to imply
  // that this is illegal.  Calling clearPath() here implements the
  // behavior apparently expected by this software.
  state->clearPath();
}

void Gfx::opConcat(Object args[], int numArgs) {
  state->concatCTM(args[0].getNum(), args[1].getNum(),
		   args[2].getNum(), args[3].getNum(),
		   args[4].getNum(), args[5].getNum());
  out->updateCTM(state, args[0].getNum(), args[1].getNum(),
		 args[2].getNum(), args[3].getNum(),
		 args[4].getNum(), args[5].getNum());
  fontChanged = gTrue;
}

void Gfx::opSetDash(Object args[], int numArgs) {
  Array *a;
  int length;
  Object obj;
  double *dash;
  int i;

  a = args[0].getArray();
  length = a->getLength();
  if (length == 0) {
    dash = NULL;
  } else {
    dash = (double *)gmalloc(length * sizeof(double));
    for (i = 0; i < length; ++i) {
      dash[i] = a->get(i, &obj)->getNum();
      obj.free();
    }
  }
  state->setLineDash(dash, length, args[1].getNum());
  out->updateLineDash(state);
}

void Gfx::opSetFlat(Object args[], int numArgs) {
  state->setFlatness((int)args[0].getNum());
  out->updateFlatness(state);
}

void Gfx::opSetLineJoin(Object args[], int numArgs) {
  state->setLineJoin(args[0].getInt());
  out->updateLineJoin(state);
}

void Gfx::opSetLineCap(Object args[], int numArgs) {
  state->setLineCap(args[0].getInt());
  out->updateLineCap(state);
}

void Gfx::opSetMiterLimit(Object args[], int numArgs) {
  state->setMiterLimit(args[0].getNum());
  out->updateMiterLimit(state);
}

void Gfx::opSetLineWidth(Object args[], int numArgs) {
  state->setLineWidth(args[0].getNum());
  out->updateLineWidth(state);
}

void Gfx::opSetExtGState(Object args[], int numArgs) {
  Object obj1, obj2;

  if (!res->lookupGState(args[0].getName(), &obj1)) {
    return;
  }
  if (!obj1.isDict()) {
    error(getPos(), "ExtGState '%s' is wrong type", args[0].getName());
    obj1.free();
    return;
  }
  if (obj1.dictLookup("ca", &obj2)->isNum()) {
    state->setFillOpacity(obj2.getNum());
    out->updateFillOpacity(state);
  }
  obj2.free();
  if (obj1.dictLookup("CA", &obj2)->isNum()) {
    state->setStrokeOpacity(obj2.getNum());
    out->updateStrokeOpacity(state);
  }
  obj2.free();
  obj1.free();
}

void Gfx::opSetRenderingIntent(Object args[], int numArgs) {
}

//------------------------------------------------------------------------
// color operators
//------------------------------------------------------------------------

void Gfx::opSetFillGray(Object args[], int numArgs) {
  GfxColor color;

  state->setFillPattern(NULL);
  state->setFillColorSpace(new GfxDeviceGrayColorSpace());
  color.c[0] = args[0].getNum();
  state->setFillColor(&color);
  out->updateFillColor(state);
}

void Gfx::opSetStrokeGray(Object args[], int numArgs) {
  GfxColor color;

  state->setStrokePattern(NULL);
  state->setStrokeColorSpace(new GfxDeviceGrayColorSpace());
  color.c[0] = args[0].getNum();
  state->setStrokeColor(&color);
  out->updateStrokeColor(state);
}

void Gfx::opSetFillCMYKColor(Object args[], int numArgs) {
  GfxColor color;
  int i;

  state->setFillPattern(NULL);
  state->setFillColorSpace(new GfxDeviceCMYKColorSpace());
  for (i = 0; i < 4; ++i) {
    color.c[i] = args[i].getNum();
  }
  state->setFillColor(&color);
  out->updateFillColor(state);
}

void Gfx::opSetStrokeCMYKColor(Object args[], int numArgs) {
  GfxColor color;
  int i;

  state->setStrokePattern(NULL);
  state->setStrokeColorSpace(new GfxDeviceCMYKColorSpace());
  for (i = 0; i < 4; ++i) {
    color.c[i] = args[i].getNum();
  }
  state->setStrokeColor(&color);
  out->updateStrokeColor(state);
}

void Gfx::opSetFillRGBColor(Object args[], int numArgs) {
  GfxColor color;
  int i;

  state->setFillPattern(NULL);
  state->setFillColorSpace(new GfxDeviceRGBColorSpace());
  for (i = 0; i < 3; ++i) {
    color.c[i] = args[i].getNum();
  }
  state->setFillColor(&color);
  out->updateFillColor(state);
}

void Gfx::opSetStrokeRGBColor(Object args[], int numArgs) {
  GfxColor color;
  int i;

  state->setStrokePattern(NULL);
  state->setStrokeColorSpace(new GfxDeviceRGBColorSpace());
  for (i = 0; i < 3; ++i) {
    color.c[i] = args[i].getNum();
  }
  state->setStrokeColor(&color);
  out->updateStrokeColor(state);
}

void Gfx::opSetFillColorSpace(Object args[], int numArgs) {
  Object obj;
  GfxColorSpace *colorSpace;
  GfxColor color;
  int i;

  state->setFillPattern(NULL);
  res->lookupColorSpace(args[0].getName(), &obj);
  if (obj.isNull()) {
    colorSpace = GfxColorSpace::parse(&args[0]);
  } else {
    colorSpace = GfxColorSpace::parse(&obj);
  }
  obj.free();
  if (colorSpace) {
    state->setFillColorSpace(colorSpace);
  } else {
    error(getPos(), "Bad color space (fill)");
  }
  for (i = 0; i < gfxColorMaxComps; ++i) {
    color.c[i] = 0;
  }
  state->setFillColor(&color);
  out->updateFillColor(state);
}

void Gfx::opSetStrokeColorSpace(Object args[], int numArgs) {
  Object obj;
  GfxColorSpace *colorSpace;
  GfxColor color;
  int i;

  state->setStrokePattern(NULL);
  res->lookupColorSpace(args[0].getName(), &obj);
  if (obj.isNull()) {
    colorSpace = GfxColorSpace::parse(&args[0]);
  } else {
    colorSpace = GfxColorSpace::parse(&obj);
  }
  obj.free();
  if (colorSpace) {
    state->setStrokeColorSpace(colorSpace);
  } else {
    error(getPos(), "Bad color space (stroke)");
  }
  for (i = 0; i < gfxColorMaxComps; ++i) {
    color.c[i] = 0;
  }
  state->setStrokeColor(&color);
  out->updateStrokeColor(state);
}

void Gfx::opSetFillColor(Object args[], int numArgs) {
  GfxColor color;
  int i;

  state->setFillPattern(NULL);
  for (i = 0; i < numArgs; ++i) {
    color.c[i] = args[i].getNum();
  }
  state->setFillColor(&color);
  out->updateFillColor(state);
}

void Gfx::opSetStrokeColor(Object args[], int numArgs) {
  GfxColor color;
  int i;

  state->setStrokePattern(NULL);
  for (i = 0; i < numArgs; ++i) {
    color.c[i] = args[i].getNum();
  }
  state->setStrokeColor(&color);
  out->updateStrokeColor(state);
}

void Gfx::opSetFillColorN(Object args[], int numArgs) {
  GfxColor color;
  GfxPattern *pattern;
  int i;

  if (state->getFillColorSpace()->getMode() == csPattern) {
    if (numArgs > 1) {
      for (i = 0; i < numArgs && i < 4; ++i) {
	if (args[i].isNum()) {
	  color.c[i] = args[i].getNum();
	}
      }
      state->setFillColor(&color);
      out->updateFillColor(state);
    }
    if (args[numArgs-1].isName() &&
	(pattern = res->lookupPattern(args[numArgs-1].getName()))) {
      state->setFillPattern(pattern);
    }

  } else {
    state->setFillPattern(NULL);
    for (i = 0; i < numArgs && i < 4; ++i) {
      if (args[i].isNum()) {
	color.c[i] = args[i].getNum();
      }
    }
    state->setFillColor(&color);
    out->updateFillColor(state);
  }
}

void Gfx::opSetStrokeColorN(Object args[], int numArgs) {
  GfxColor color;
  GfxPattern *pattern;
  int i;

  if (state->getStrokeColorSpace()->getMode() == csPattern) {
    if (numArgs > 1) {
      for (i = 0; i < numArgs && i < 4; ++i) {
	if (args[i].isNum()) {
	  color.c[i] = args[i].getNum();
	}
      }
      state->setStrokeColor(&color);
      out->updateStrokeColor(state);
    }
    if (args[numArgs-1].isName() &&
	(pattern = res->lookupPattern(args[numArgs-1].getName()))) {
      state->setStrokePattern(pattern);
    }

  } else {
    state->setStrokePattern(NULL);
    for (i = 0; i < numArgs && i < 4; ++i) {
      if (args[i].isNum()) {
	color.c[i] = args[i].getNum();
      }
    }
    state->setStrokeColor(&color);
    out->updateStrokeColor(state);
  }
}

//------------------------------------------------------------------------
// path segment operators
//------------------------------------------------------------------------

void Gfx::opMoveTo(Object args[], int numArgs) {
  state->moveTo(args[0].getNum(), args[1].getNum());
}

void Gfx::opLineTo(Object args[], int numArgs) {
  if (!state->isCurPt()) {
    error(getPos(), "No current point in lineto");
    return;
  }
  state->lineTo(args[0].getNum(), args[1].getNum());
}

void Gfx::opCurveTo(Object args[], int numArgs) {
  double x1, y1, x2, y2, x3, y3;

  if (!state->isCurPt()) {
    error(getPos(), "No current point in curveto");
    return;
  }
  x1 = args[0].getNum();
  y1 = args[1].getNum();
  x2 = args[2].getNum();
  y2 = args[3].getNum();
  x3 = args[4].getNum();
  y3 = args[5].getNum();
  state->curveTo(x1, y1, x2, y2, x3, y3);
}

void Gfx::opCurveTo1(Object args[], int numArgs) {
  double x1, y1, x2, y2, x3, y3;

  if (!state->isCurPt()) {
    error(getPos(), "No current point in curveto1");
    return;
  }
  x1 = state->getCurX();
  y1 = state->getCurY();
  x2 = args[0].getNum();
  y2 = args[1].getNum();
  x3 = args[2].getNum();
  y3 = args[3].getNum();
  state->curveTo(x1, y1, x2, y2, x3, y3);
}

void Gfx::opCurveTo2(Object args[], int numArgs) {
  double x1, y1, x2, y2, x3, y3;

  if (!state->isCurPt()) {
    error(getPos(), "No current point in curveto2");
    return;
  }
  x1 = args[0].getNum();
  y1 = args[1].getNum();
  x2 = args[2].getNum();
  y2 = args[3].getNum();
  x3 = x2;
  y3 = y2;
  state->curveTo(x1, y1, x2, y2, x3, y3);
}

void Gfx::opRectangle(Object args[], int numArgs) {
  double x, y, w, h;

  x = args[0].getNum();
  y = args[1].getNum();
  w = args[2].getNum();
  h = args[3].getNum();
  state->moveTo(x, y);
  state->lineTo(x + w, y);
  state->lineTo(x + w, y + h);
  state->lineTo(x, y + h);
  state->closePath();
}

void Gfx::opClosePath(Object args[], int numArgs) {
  if (!state->isPath()) {
    error(getPos(), "No current point in closepath");
    return;
  }
  state->closePath();
}

//------------------------------------------------------------------------
// path painting operators
//------------------------------------------------------------------------

void Gfx::opEndPath(Object args[], int numArgs) {
  doEndPath();
}

void Gfx::opStroke(Object args[], int numArgs) {
  if (!state->isCurPt()) {
    //error(getPos(), "No path in stroke");
    return;
  }
  if (state->isPath())
    out->stroke(state);
  doEndPath();
}

void Gfx::opCloseStroke(Object args[], int numArgs) {
  if (!state->isCurPt()) {
    //error(getPos(), "No path in closepath/stroke");
    return;
  }
  if (state->isPath()) {
    state->closePath();
    out->stroke(state);
  }
  doEndPath();
}

void Gfx::opFill(Object args[], int numArgs) {
  if (!state->isCurPt()) {
    //error(getPos(), "No path in fill");
    return;
  }
  if (state->isPath()) {
    if (state->getFillColorSpace()->getMode() == csPattern) {
      doPatternFill(gFalse);
    } else {
      out->fill(state);
    }
  }
  doEndPath();
}

void Gfx::opEOFill(Object args[], int numArgs) {
  if (!state->isCurPt()) {
    //error(getPos(), "No path in eofill");
    return;
  }
  if (state->isPath()) {
    if (state->getFillColorSpace()->getMode() == csPattern) {
      doPatternFill(gTrue);
    } else {
      out->eoFill(state);
    }
  }
  doEndPath();
}

void Gfx::opFillStroke(Object args[], int numArgs) {
  if (!state->isCurPt()) {
    //error(getPos(), "No path in fill/stroke");
    return;
  }
  if (state->isPath()) {
    if (state->getFillColorSpace()->getMode() == csPattern) {
      doPatternFill(gFalse);
    } else {
      out->fill(state);
    }
    out->stroke(state);
  }
  doEndPath();
}

void Gfx::opCloseFillStroke(Object args[], int numArgs) {
  if (!state->isCurPt()) {
    //error(getPos(), "No path in closepath/fill/stroke");
    return;
  }
  if (state->isPath()) {
    state->closePath();
    if (state->getFillColorSpace()->getMode() == csPattern) {
      doPatternFill(gFalse);
    } else {
      out->fill(state);
    }
    out->stroke(state);
  }
  doEndPath();
}

void Gfx::opEOFillStroke(Object args[], int numArgs) {
  if (!state->isCurPt()) {
    //error(getPos(), "No path in eofill/stroke");
    return;
  }
  if (state->isPath()) {
    if (state->getFillColorSpace()->getMode() == csPattern) {
      doPatternFill(gTrue);
    } else {
      out->eoFill(state);
    }
    out->stroke(state);
  }
  doEndPath();
}

void Gfx::opCloseEOFillStroke(Object args[], int numArgs) {
  if (!state->isCurPt()) {
    //error(getPos(), "No path in closepath/eofill/stroke");
    return;
  }
  if (state->isPath()) {
    state->closePath();
    if (state->getFillColorSpace()->getMode() == csPattern) {
      doPatternFill(gTrue);
    } else {
      out->eoFill(state);
    }
    out->stroke(state);
  }
  doEndPath();
}

void Gfx::doPatternFill(GBool eoFill) {
  GfxPatternColorSpace *patCS;
  GfxPattern *pattern;
  GfxTilingPattern *tPat;
  GfxColorSpace *cs;
  double xMin, yMin, xMax, yMax, x, y, x1, y1;
  double cxMin, cyMin, cxMax, cyMax;
  int xi0, yi0, xi1, yi1, xi, yi;
  double *ctm, *btm, *ptm;
  double m[6], ictm[6], m1[6], im[6], imb[6];
  double det;
  double xstep, ystep;
  int i;

  // get color space
  patCS = (GfxPatternColorSpace *)state->getFillColorSpace();

  // get pattern
  if (!(pattern = state->getFillPattern())) {
    return;
  }
  if (pattern->getType() != 1) {
    return;
  }
  tPat = (GfxTilingPattern *)pattern;

  // construct a (pattern space) -> (current space) transform matrix
  ctm = state->getCTM();
  btm = baseMatrix;
  ptm = tPat->getMatrix();
  // iCTM = invert CTM
  det = 1 / (ctm[0] * ctm[3] - ctm[1] * ctm[2]);
  ictm[0] = ctm[3] * det;
  ictm[1] = -ctm[1] * det;
  ictm[2] = -ctm[2] * det;
  ictm[3] = ctm[0] * det;
  ictm[4] = (ctm[2] * ctm[5] - ctm[3] * ctm[4]) * det;
  ictm[5] = (ctm[1] * ctm[4] - ctm[0] * ctm[5]) * det;
  // m1 = PTM * BTM = PTM * base transform matrix
  m1[0] = ptm[0] * btm[0] + ptm[1] * btm[2];
  m1[1] = ptm[0] * btm[1] + ptm[1] * btm[3];
  m1[2] = ptm[2] * btm[0] + ptm[3] * btm[2];
  m1[3] = ptm[2] * btm[1] + ptm[3] * btm[3];
  m1[4] = ptm[4] * btm[0] + ptm[5] * btm[2] + btm[4];
  m1[5] = ptm[4] * btm[1] + ptm[5] * btm[3] + btm[5];
  // m = m1 * iCTM = (PTM * BTM) * (iCTM)
  m[0] = m1[0] * ictm[0] + m1[1] * ictm[2];
  m[1] = m1[0] * ictm[1] + m1[1] * ictm[3];
  m[2] = m1[2] * ictm[0] + m1[3] * ictm[2];
  m[3] = m1[2] * ictm[1] + m1[3] * ictm[3];
  m[4] = m1[4] * ictm[0] + m1[5] * ictm[2] + ictm[4];
  m[5] = m1[4] * ictm[1] + m1[5] * ictm[3] + ictm[5];

  // construct a (current space) -> (pattern space) transform matrix
  det = 1 / (m[0] * m[3] - m[1] * m[2]);
  im[0] = m[3] * det;
  im[1] = -m[1] * det;
  im[2] = -m[2] * det;
  im[3] = m[0] * det;
  im[4] = (m[2] * m[5] - m[3] * m[4]) * det;
  im[5] = (m[1] * m[4] - m[0] * m[5]) * det;

  // construct a (base space) -> (pattern space) transform matrix
  det = 1 / (m1[0] * m1[3] - m1[1] * m1[2]);
  imb[0] = m1[3] * det;
  imb[1] = -m1[1] * det;
  imb[2] = -m1[2] * det;
  imb[3] = m1[0] * det;
  imb[4] = (m1[2] * m1[5] - m1[3] * m1[4]) * det;
  imb[5] = (m1[1] * m1[4] - m1[0] * m1[5]) * det;

  // save current graphics state
  out->saveState(state);
  state = state->save();

  // set underlying color space (for uncolored tiling patterns)
  if (tPat->getPaintType() == 2 && (cs = patCS->getUnder())) {
    state->setFillColorSpace(cs->copy());
  } else {
    state->setFillColorSpace(new GfxDeviceGrayColorSpace());
  }
  state->setFillPattern(NULL);
  out->updateFillColor(state);

  // clip to current path
  state->clip();
  if (eoFill) {
    out->eoClip(state);
  } else {
    out->clip(state);
  }
  state->clearPath();

  // transform clip region bbox to pattern space
  state->getClipBBox(&cxMin, &cyMin, &cxMax, &cyMax);
  xMin = xMax = cxMin * imb[0] + cyMin * imb[2] + imb[4];
  yMin = yMax = cxMin * imb[1] + cyMin * imb[3] + imb[5];
  x1 = cxMin * imb[0] + cyMax * imb[2] + imb[4];
  y1 = cxMin * imb[1] + cyMax * imb[3] + imb[5];
  if (x1 < xMin) {
    xMin = x1;
  } else if (x1 > xMax) {
    xMax = x1;
  }
  if (y1 < yMin) {
    yMin = y1;
  } else if (y1 > yMax) {
    yMax = y1;
  }
  x1 = cxMax * imb[0] + cyMin * imb[2] + imb[4];
  y1 = cxMax * imb[1] + cyMin * imb[3] + imb[5];
  if (x1 < xMin) {
    xMin = x1;
  } else if (x1 > xMax) {
    xMax = x1;
  }
  if (y1 < yMin) {
    yMin = y1;
  } else if (y1 > yMax) {
    yMax = y1;
  }
  x1 = cxMax * imb[0] + cyMax * imb[2] + imb[4];
  y1 = cxMax * imb[1] + cyMax * imb[3] + imb[5];
  if (x1 < xMin) {
    xMin = x1;
  } else if (x1 > xMax) {
    xMax = x1;
  }
  if (y1 < yMin) {
    yMin = y1;
  } else if (y1 > yMax) {
    yMax = y1;
  }

  // draw the pattern
  //~ this should treat negative steps differently -- start at right/top
  //~ edge instead of left/bottom (?)
  xstep = fabs(tPat->getXStep());
  ystep = fabs(tPat->getYStep());
  xi0 = (int)floor(xMin / xstep);
  xi1 = (int)ceil(xMax / xstep);
  yi0 = (int)floor(yMin / ystep);
  yi1 = (int)ceil(yMax / ystep);
  for (i = 0; i < 4; ++i) {
    m1[i] = m[i];
  }
  for (yi = yi0; yi < yi1; ++yi) {
    for (xi = xi0; xi < xi1; ++xi) {
      x = xi * xstep;
      y = yi * ystep;
      m1[4] = x * m[0] + y * m[2] + m[4];
      m1[5] = x * m[1] + y * m[3] + m[5];
      doForm1(tPat->getContentStream(), tPat->getResDict(),
	      m1, tPat->getBBox());
    }
  }

  // restore graphics state
  state = state->restore();
  out->restoreState(state);
}

void Gfx::opShFill(Object args[], int numArgs) {
  GfxShading *shading;
  double xMin, yMin, xMax, yMax;

  if (!(shading = res->lookupShading(args[0].getName()))) {
    return;
  }

  // save current graphics state
  out->saveState(state);
  state = state->save();

  // clip to bbox
  if (shading->getHasBBox()) {
    shading->getBBox(&xMin, &yMin, &xMax, &yMax);
    state->moveTo(xMin, yMin);
    state->lineTo(xMax, yMin);
    state->lineTo(xMax, yMax);
    state->lineTo(xMin, yMax);
    state->closePath();
    state->clip();
    out->clip(state);
    state->clearPath();
  }

  // set the color space
  state->setFillColorSpace(shading->getColorSpace()->copy());

  // do shading type-specific operations
  switch (shading->getType()) {
  case 2:
    doAxialShFill((GfxAxialShading *)shading);
    break;
  case 3:
    doRadialShFill((GfxRadialShading *)shading);
    break;
  }

  // restore graphics state
  state = state->restore();
  out->restoreState(state);

  delete shading;
}

void Gfx::doAxialShFill(GfxAxialShading *shading) {
  double xMin, yMin, xMax, yMax;
  double x0, y0, x1, y1;
  double det;
  double *ctm;
  double ictm[6];
  double dx, dy, mul;
  double tMin, tMax, t, tx, ty;
  double s[4], sMin, sMax, tmp;
  double ux0, uy0, ux1, uy1, vx0, vy0, vx1, vy1;
  double t0, t1, tt;
  double ta[axialMaxSplits + 1];
  int next[axialMaxSplits + 1];
  GfxColor color0, color1;
  int nComps;
  int i, j, k, kk;

  // get clip region bbox and transform to current user space
  state->getClipBBox(&x0, &y0, &x1, &y1);
  ctm = state->getCTM();
  det = 1 / (ctm[0] * ctm[3] - ctm[1] * ctm[2]);
  ictm[0] = ctm[3] * det;
  ictm[1] = -ctm[1] * det;
  ictm[2] = -ctm[2] * det;
  ictm[3] = ctm[0] * det;
  ictm[4] = (ctm[2] * ctm[5] - ctm[3] * ctm[4]) * det;
  ictm[5] = (ctm[1] * ctm[4] - ctm[0] * ctm[5]) * det;
  xMin = xMax = x0 * ictm[0] + y0 * ictm[2] + ictm[4];
  yMin = yMax = x0 * ictm[1] + y0 * ictm[3] + ictm[5];
  tx = x0 * ictm[0] + y1 * ictm[2] + ictm[4];
  ty = x0 * ictm[1] + y1 * ictm[3] + ictm[5];
  if (tx < xMin) {
    xMin = tx;
  } else if (tx > xMax) {
    xMax = tx;
  }
  if (ty < yMin) {
    yMin = ty;
  } else if (ty > yMax) {
    yMax = ty;
  }
  tx = x1 * ictm[0] + y0 * ictm[2] + ictm[4];
  ty = x1 * ictm[1] + y0 * ictm[3] + ictm[5];
  if (tx < xMin) {
    xMin = tx;
  } else if (tx > xMax) {
    xMax = tx;
  }
  if (ty < yMin) {
    yMin = ty;
  } else if (ty > yMax) {
    yMax = ty;
  }
  tx = x1 * ictm[0] + y1 * ictm[2] + ictm[4];
  ty = x1 * ictm[1] + y1 * ictm[3] + ictm[5];
  if (tx < xMin) {
    xMin = tx;
  } else if (tx > xMax) {
    xMax = tx;
  }
  if (ty < yMin) {
    yMin = ty;
  } else if (ty > yMax) {
    yMax = ty;
  }

  // compute min and max t values, based on the four corners of the
  // clip region bbox
  shading->getCoords(&x0, &y0, &x1, &y1);
  dx = x1 - x0;
  dy = y1 - y0;
  mul = 1 / (dx * dx + dy * dy);
  tMin = tMax = ((xMin - x0) * dx + (yMin - y0) * dy) * mul;
  t = ((xMin - x0) * dx + (yMax - y0) * dy) * mul;
  if (t < tMin) {
    tMin = t;
  } else if (t > tMax) {
    tMax = t;
  }
  t = ((xMax - x0) * dx + (yMin - y0) * dy) * mul;
  if (t < tMin) {
    tMin = t;
  } else if (t > tMax) {
    tMax = t;
  }
  t = ((xMax - x0) * dx + (yMax - y0) * dy) * mul;
  if (t < tMin) {
    tMin = t;
  } else if (t > tMax) {
    tMax = t;
  }
  if (tMin < 0 && !shading->getExtend0()) {
    tMin = 0;
  }
  if (tMax > 1 && !shading->getExtend1()) {
    tMax = 1;
  }

  // get the function domain
  t0 = shading->getDomain0();
  t1 = shading->getDomain1();

  // Traverse the t axis and do the shading.
  //
  // For each point (tx, ty) on the t axis, consider a line through
  // that point perpendicular to the t axis:
  //
  //     x(s) = tx + s * -dy   -->   s = (x - tx) / -dy
  //     y(s) = ty + s * dx    -->   s = (y - ty) / dx
  //
  // Then look at the intersection of this line with the bounding box
  // (xMin, yMin, xMax, yMax).  In the general case, there are four
  // intersection points:
  //
  //     s0 = (xMin - tx) / -dy
  //     s1 = (xMax - tx) / -dy
  //     s2 = (yMin - ty) / dx
  //     s3 = (yMax - ty) / dx
  //
  // and we want the middle two s values.
  //
  // In the case where dx = 0, take s0 and s1; in the case where dy =
  // 0, take s2 and s3.
  //
  // Each filled polygon is bounded by two of these line segments
  // perpdendicular to the t axis.
  //
  // The t axis is bisected into smaller regions until the color
  // difference across a region is small enough, and then the region
  // is painted with a single color.

  // set up
  nComps = shading->getColorSpace()->getNComps();
  ta[0] = tMin;
  ta[axialMaxSplits] = tMax;
  next[0] = axialMaxSplits;

  // compute the color at t = tMin
  if (tMin < 0) {
    tt = t0;
  } else if (tMin > 1) {
    tt = t1;
  } else {
    tt = t0 + (t1 - t0) * tMin;
  }
  shading->getColor(tt, &color0);

  // compute the coordinates of the point on the t axis at t = tMin;
  // then compute the intersection of the perpendicular line with the
  // bounding box
  tx = x0 + tMin * dx;
  ty = y0 + tMin * dy;
  if (dx == 0 && dy == 0) {
    sMin = sMax = 0;
  } if (dx == 0) {
    sMin = (xMin - tx) / -dy;
    sMax = (xMax - tx) / -dy;
    if (sMin > sMax) { tmp = sMin; sMin = sMax; sMax = tmp; }
  } else if (dy == 0) {
    sMin = (yMin - ty) / dx;
    sMax = (yMax - ty) / dx;
    if (sMin > sMax) { tmp = sMin; sMin = sMax; sMax = tmp; }
  } else {
    s[0] = (yMin - ty) / dx;
    s[1] = (yMax - ty) / dx;
    s[2] = (xMin - tx) / -dy;
    s[3] = (xMax - tx) / -dy;
    for (j = 0; j < 3; ++j) {
      kk = j;
      for (k = j + 1; k < 4; ++k) {
	if (s[k] < s[kk]) {
	  kk = k;
	}
      }
      tmp = s[j]; s[j] = s[kk]; s[kk] = tmp;
    }
    sMin = s[1];
    sMax = s[2];
  }
  ux0 = tx - sMin * dy;
  uy0 = ty + sMin * dx;
  vx0 = tx - sMax * dy;
  vy0 = ty + sMax * dx;

  i = 0;
  while (i < axialMaxSplits) {

    // bisect until color difference is small enough or we hit the
    // bisection limit
    j = next[i];
    while (j > i + 1) {
      if (ta[j] < 0) {
	tt = t0;
      } else if (ta[j] > 1) {
	tt = t1;
      } else {
	tt = t0 + (t1 - t0) * ta[j];
      }
      shading->getColor(tt, &color1);
      for (k = 0; k < nComps; ++k) {
	if (fabs(color1.c[k] - color0.c[k]) > axialColorDelta) {
	  break;
	}
      }
      if (k == nComps) {
	break;
      }
      k = (i + j) / 2;
      ta[k] = 0.5 * (ta[i] + ta[j]);
      next[i] = k;
      next[k] = j;
      j = k;
    }

    // use the average of the colors of the two sides of the region
    for (k = 0; k < nComps; ++k) {
      color0.c[k] = 0.5 * (color0.c[k] + color1.c[k]);
    }

    // compute the coordinates of the point on the t axis; then
    // compute the intersection of the perpendicular line with the
    // bounding box
    tx = x0 + ta[j] * dx;
    ty = y0 + ta[j] * dy;
    if (dx == 0 && dy == 0) {
      sMin = sMax = 0;
    } if (dx == 0) {
      sMin = (xMin - tx) / -dy;
      sMax = (xMax - tx) / -dy;
      if (sMin > sMax) { tmp = sMin; sMin = sMax; sMax = tmp; }
    } else if (dy == 0) {
      sMin = (yMin - ty) / dx;
      sMax = (yMax - ty) / dx;
      if (sMin > sMax) { tmp = sMin; sMin = sMax; sMax = tmp; }
    } else {
      s[0] = (yMin - ty) / dx;
      s[1] = (yMax - ty) / dx;
      s[2] = (xMin - tx) / -dy;
      s[3] = (xMax - tx) / -dy;
      for (j = 0; j < 3; ++j) {
	kk = j;
	for (k = j + 1; k < 4; ++k) {
	  if (s[k] < s[kk]) {
	    kk = k;
	  }
	}
	tmp = s[j]; s[j] = s[kk]; s[kk] = tmp;
      }
      sMin = s[1];
      sMax = s[2];
    }
    ux1 = tx - sMin * dy;
    uy1 = ty + sMin * dx;
    vx1 = tx - sMax * dy;
    vy1 = ty + sMax * dx;

    // set the color
    state->setFillColor(&color0);
    out->updateFillColor(state);

    // fill the region
    state->moveTo(ux0, uy0);
    state->lineTo(vx0, vy0);
    state->lineTo(vx1, vy1);
    state->lineTo(ux1, uy1);
    state->closePath();
    out->fill(state);
    state->clearPath();

    // set up for next region
    ux0 = ux1;
    uy0 = uy1;
    vx0 = vx1;
    vy0 = vy1;
    color0 = color1;
    i = next[i];
  }
}

void Gfx::doRadialShFill(GfxRadialShading *shading) {
  double x0, y0, x1, y1, r0, r1;
  double xx, yy, rr, dr, dt;
  double cx, cy, th;
  double t0, t1, tt;
  GfxColor color;

  // Find the centers and radii of the two circles...
  shading->getCoords(&x0, &y0, &x1, &y1);
  shading->getRadii(&r0, &r1);

  if (r0 == 0.0f && r1 == 0.0f) return;

  // get the function domain
  t0 = shading->getDomain0();
  t1 = shading->getDomain1();

  // draw circles, stepping in small increments...
  for (dr = (r1 - r0); dr > 0.1; dr *= 0.1);
  if (dr < 0.001) dr = 1.0;

  for (rr = r1; rr >= r0; rr -= dr) {
    // get the current center/color
    dt = (rr - r0) / (r1 - r0);

    xx = x0 + (x1 - x0) * dt;
    yy = y0 + (y1 - y0) * dt;
    tt = t0 + (t1 - t0) * dt;

    shading->getColor(tt, &color);

    // set the color
    state->setFillColor(&color);
    out->updateFillColor(state);

    // stroke the circle
    for (th = 0.0; th < 2 * M_PI; th += M_PI * 0.05) {
      cx = xx + rr * cos(th);
      cy = yy + rr * sin(th);

      if (th == 0.0) state->moveTo(cx, cy);
      else state->lineTo(cx, cy);
    }

    state->closePath();
    out->fill(state);
    state->clearPath();
  }
}

void Gfx::doEndPath() {
  if (state->isPath() && clip != clipNone) {
    state->clip();
    if (clip == clipNormal) {
      out->clip(state);
    } else {
      out->eoClip(state);
    }
  }
  clip = clipNone;
  state->clearPath();
}

//------------------------------------------------------------------------
// path clipping operators
//------------------------------------------------------------------------

void Gfx::opClip(Object args[], int numArgs) {
  clip = clipNormal;
}

void Gfx::opEOClip(Object args[], int numArgs) {
  clip = clipEO;
}

//------------------------------------------------------------------------
// text object operators
//------------------------------------------------------------------------

void Gfx::opBeginText(Object args[], int numArgs) {
  state->setTextMat(1, 0, 0, 1, 0, 0);
  state->textMoveTo(0, 0);
  out->updateTextMat(state);
  out->updateTextPos(state);
  fontChanged = gTrue;
}

void Gfx::opEndText(Object args[], int numArgs) {
}

//------------------------------------------------------------------------
// text state operators
//------------------------------------------------------------------------

void Gfx::opSetCharSpacing(Object args[], int numArgs) {
  state->setCharSpace(args[0].getNum());
  out->updateCharSpace(state);
}

void Gfx::opSetFont(Object args[], int numArgs) {
  GfxFont *font;

  if (!(font = res->lookupFont(args[0].getName()))) {
    return;
  }
  if (printCommands) {
    printf("  font: '%s' %g\n",
	   font->getName() ? font->getName()->getCString() : "???",
	   args[1].getNum());
    fflush(stdout);
  }
  state->setFont(font, args[1].getNum());
  fontChanged = gTrue;
}

void Gfx::opSetTextLeading(Object args[], int numArgs) {
  state->setLeading(args[0].getNum());
}

void Gfx::opSetTextRender(Object args[], int numArgs) {
  state->setRender(args[0].getInt());
  out->updateRender(state);
}

void Gfx::opSetTextRise(Object args[], int numArgs) {
  state->setRise(args[0].getNum());
  out->updateRise(state);
}

void Gfx::opSetWordSpacing(Object args[], int numArgs) {
  state->setWordSpace(args[0].getNum());
  out->updateWordSpace(state);
}

void Gfx::opSetHorizScaling(Object args[], int numArgs) {
  state->setHorizScaling(args[0].getNum());
  out->updateHorizScaling(state);
  fontChanged = gTrue;
}

//------------------------------------------------------------------------
// text positioning operators
//------------------------------------------------------------------------

void Gfx::opTextMove(Object args[], int numArgs) {
  double tx, ty;

  tx = state->getLineX() + args[0].getNum();
  ty = state->getLineY() + args[1].getNum();
  state->textMoveTo(tx, ty);
  out->updateTextPos(state);
}

void Gfx::opTextMoveSet(Object args[], int numArgs) {
  double tx, ty;

  tx = state->getLineX() + args[0].getNum();
  ty = args[1].getNum();
  state->setLeading(-ty);
  ty += state->getLineY();
  state->textMoveTo(tx, ty);
  out->updateTextPos(state);
}

void Gfx::opSetTextMatrix(Object args[], int numArgs) {
  state->setTextMat(args[0].getNum(), args[1].getNum(),
		    args[2].getNum(), args[3].getNum(),
		    args[4].getNum(), args[5].getNum());
  state->textMoveTo(0, 0);
  out->updateTextMat(state);
  out->updateTextPos(state);
  fontChanged = gTrue;
}

void Gfx::opTextNextLine(Object args[], int numArgs) {
  double tx, ty;

  tx = state->getLineX();
  ty = state->getLineY() - state->getLeading();
  state->textMoveTo(tx, ty);
  out->updateTextPos(state);
}

//------------------------------------------------------------------------
// text string operators
//------------------------------------------------------------------------

void Gfx::opShowText(Object args[], int numArgs) {
  if (!state->getFont()) {
    error(getPos(), "No font in show");
    return;
  }
  doShowText(args[0].getString());
}

void Gfx::opMoveShowText(Object args[], int numArgs) {
  double tx, ty;

  if (!state->getFont()) {
    error(getPos(), "No font in move/show");
    return;
  }
  tx = state->getLineX();
  ty = state->getLineY() - state->getLeading();
  state->textMoveTo(tx, ty);
  out->updateTextPos(state);
  doShowText(args[0].getString());
}

void Gfx::opMoveSetShowText(Object args[], int numArgs) {
  double tx, ty;

  if (!state->getFont()) {
    error(getPos(), "No font in move/set/show");
    return;
  }
  state->setWordSpace(args[0].getNum());
  state->setCharSpace(args[1].getNum());
  tx = state->getLineX();
  ty = state->getLineY() - state->getLeading();
  state->textMoveTo(tx, ty);
  out->updateWordSpace(state);
  out->updateCharSpace(state);
  out->updateTextPos(state);
  doShowText(args[2].getString());
}

void Gfx::opShowSpaceText(Object args[], int numArgs) {
  Array *a;
  Object obj;
  int i;

  if (!state->getFont()) {
    error(getPos(), "No font in show/space");
    return;
  }
  a = args[0].getArray();
  for (i = 0; i < a->getLength(); ++i) {
    a->get(i, &obj);
    if (obj.isNum()) {
      state->textShift(-obj.getNum() * 0.001 * state->getFontSize());
      out->updateTextShift(state, obj.getNum());
    } else if (obj.isString()) {
      doShowText(obj.getString());
    } else {
      error(getPos(), "Element of show/space array must be number or string");
    }
    obj.free();
  }
}

void Gfx::doShowText(GString *s) {
  GfxFont *font;
  GfxFontEncoding16 *enc;
  Guchar *p;
  Guchar c8;
  int c16;
  GString *s16;
  char s16a[2];
  int m, n;
#if 1 //~type3
  double dx, dy, width, height, w, h, x, y;
  double oldCTM[6], newCTM[6];
  double *mat;
  Object charProc;
  Parser *oldParser;
  int i;
#else
  double dx, dy, width, height, w, h;
#endif
  double sWidth, sHeight;

  if (fontChanged) {
    out->updateFont(state);
    fontChanged = gFalse;
  }
  font = state->getFont();

  //----- 16-bit font
  if (font->is16Bit()) {
    enc = font->getEncoding16();
    if (out->useDrawChar()) {
      out->beginString(state, s);
      s16 = NULL;
    } else {
      s16 = new GString();
    }
    sWidth = sHeight = 0;
    state->textTransformDelta(0, state->getRise(), &dx, &dy);
    p = (Guchar *)s->getCString();
    n = s->getLength();
    while (n > 0) {
      m = getNextChar16(enc, p, &c16);
      if (enc->wMode == 0) {
	width = state->getFontSize() * font->getWidth16(c16) +
	        state->getCharSpace();
	if (m == 1 && c16 == ' ') {
	  width += state->getWordSpace();
	}
	width *= state->getHorizScaling();
	height = 0;
      } else {
	width = 0;
	height = state->getFontSize() * font->getHeight16(c16);
      }
      state->textTransformDelta(width, height, &w, &h);
      if (out->useDrawChar()) {
	out->drawChar16(state, state->getCurX() + dx, state->getCurY() + dy,
			w, h, c16);
	state->textShift(width, height);
      } else {
	s16a[0] = (char)(c16 >> 8);
	s16a[1] = (char)c16;
	s16->append(s16a, 2);
	sWidth += w;
	sHeight += h;
      }
      n -= m;
      p += m;
    }
    if (out->useDrawChar()) {
      out->endString(state);
    } else {
      out->drawString16(state, s16);
      delete s16;
      state->textShift(sWidth, sHeight);
    }

  //----- 8-bit font
  } else {
#if 1 //~type3
    //~ also check out->renderType3()
    if (font->getType() == fontType3) {
      out->beginString(state, s);
      mat = state->getCTM();
      for (i = 0; i < 6; ++i) {
	oldCTM[i] = mat[i];
      }
      mat = state->getTextMat();
      newCTM[0] = mat[0] * oldCTM[0] + mat[1] * oldCTM[2];
      newCTM[1] = mat[0] * oldCTM[1] + mat[1] * oldCTM[3];
      newCTM[2] = mat[2] * oldCTM[0] + mat[3] * oldCTM[2];
      newCTM[3] = mat[2] * oldCTM[1] + mat[3] * oldCTM[3];
      mat = font->getFontMatrix();
      newCTM[0] = mat[0] * newCTM[0] + mat[1] * newCTM[2];
      newCTM[1] = mat[0] * newCTM[1] + mat[1] * newCTM[3];
      newCTM[2] = mat[2] * newCTM[0] + mat[3] * newCTM[2];
      newCTM[3] = mat[2] * newCTM[1] + mat[3] * newCTM[3];
      newCTM[0] *= state->getFontSize();
      newCTM[3] *= state->getFontSize();
      newCTM[0] *= state->getHorizScaling();
      newCTM[2] *= state->getHorizScaling();
      state->textTransformDelta(0, state->getRise(), &dx, &dy);
      oldParser = parser;
      for (p = (Guchar *)s->getCString(), n = s->getLength(); n; ++p, --n) {
	c8 = *p;
	font->getCharProc(c8, &charProc);
	state->transform(state->getCurX() + dx, state->getCurY() + dy, &x, &y);
	out->saveState(state);
	state = state->save();
	state->setCTM(newCTM[0], newCTM[1], newCTM[2], newCTM[3], x, y);
	//~ out->updateCTM(???)
	if (charProc.isStream()) {
	  display(&charProc, gFalse);
	} else {
	  error(getPos(), "Missing or bad Type3 CharProc entry");
	}
	state = state->restore();
	out->restoreState(state);
	charProc.free();
	width = state->getFontSize() * font->getWidth(c8) +
	        state->getCharSpace();
	if (c8 == ' ') {
	  width += state->getWordSpace();
	}
	width *= state->getHorizScaling();
	state->textShift(width);
      }
      parser = oldParser;
      out->endString(state);
    } else
#endif
    if (out->useDrawChar()) {
      out->beginString(state, s);
      state->textTransformDelta(0, state->getRise(), &dx, &dy);
      for (p = (Guchar *)s->getCString(), n = s->getLength(); n; ++p, --n) {
	c8 = *p;
	width = state->getFontSize() * font->getWidth(c8) +
	        state->getCharSpace();
	if (c8 == ' ') {
	  width += state->getWordSpace();
	}
	width *= state->getHorizScaling();
	state->textTransformDelta(width, 0, &w, &h);
	out->drawChar(state, state->getCurX() + dx, state->getCurY() + dy,
		      w, h, c8);
	state->textShift(width);
      }
      out->endString(state);
    } else {
      out->drawString(state, s);
      width = state->getFontSize() * font->getWidth(s) +
	      s->getLength() * state->getCharSpace();
      for (p = (Guchar *)s->getCString(), n = s->getLength(); n; ++p, --n) {
	if (*p == ' ') {
	  width += state->getWordSpace();
	}
      }
      width *= state->getHorizScaling();
      state->textShift(width);
    }
  }
}

int Gfx::getNextChar16(GfxFontEncoding16 *enc, Guchar *p, int *c16) {
  int n;
  int code;
  int a, b, m;

  n = enc->codeLen[*p];
  if (n == 1) {
    *c16 = enc->map1[*p];
  } else {
    code = (p[0] << 8) + p[1];
    a = 0;
    b = enc->map2Len;
    // invariant: map2[2*a] <= code < map2[2*b]
    while (b - a > 1) {
      m = (a + b) / 2;
      if (enc->map2[2*m] <= code)
	a = m;
      else if (enc->map2[2*m] > code)
	b = m;
      else
	break;
    }
    *c16 = enc->map2[2*a+1] + (code - enc->map2[2*a]);
  }
  return n;
}

//------------------------------------------------------------------------
// XObject operators
//------------------------------------------------------------------------

void Gfx::opXObject(Object args[], int numArgs) {
  Object obj1, obj2, refObj;
#if OPI_SUPPORT
  Object opiDict;
#endif

  if (!res->lookupXObject(args[0].getName(), &obj1)) {
    return;
  }
  if (!obj1.isStream()) {
    error(getPos(), "XObject '%s' is wrong type", args[0].getName());
    obj1.free();
    return;
  }
#if OPI_SUPPORT
  obj1.streamGetDict()->lookup("OPI", &opiDict);
  if (opiDict.isDict()) {
    out->opiBegin(state, opiDict.getDict());
  }
#endif
  obj1.streamGetDict()->lookup("Subtype", &obj2);
  if (obj2.isName("Image")) {
    res->lookupXObjectNF(args[0].getName(), &refObj);
    doImage(&refObj, obj1.getStream(), gFalse);
    refObj.free();
  } else if (obj2.isName("Form")) {
    doForm(&obj1);
  } else if (obj2.isName()) {
    error(getPos(), "Unknown XObject subtype '%s'", obj2.getName());
  } else {
    error(getPos(), "XObject subtype is missing or wrong type");
  }
  obj2.free();
#if OPI_SUPPORT
  if (opiDict.isDict()) {
    out->opiEnd(state, opiDict.getDict());
  }
  opiDict.free();
#endif
  obj1.free();
}

void Gfx::doImage(Object *ref, Stream *str, GBool inlineImg) {
  Dict *dict;
  int width, height;
  int bits;
  GBool mask;
  GBool invert;
  GfxColorSpace *colorSpace;
  GfxImageColorMap *colorMap;
  Object maskObj;
  GBool haveMask;
  int maskColors[2*gfxColorMaxComps];
  Object obj1, obj2;
  int i;

  // get stream dict
  dict = str->getDict();

  // get size
  dict->lookup("Width", &obj1);
  if (obj1.isNull()) {
    obj1.free();
    dict->lookup("W", &obj1);
  }
  if (!obj1.isInt())
    goto err2;
  width = obj1.getInt();
  obj1.free();
  dict->lookup("Height", &obj1);
  if (obj1.isNull()) {
    obj1.free();
    dict->lookup("H", &obj1);
  }
  if (!obj1.isInt())
    goto err2;
  height = obj1.getInt();
  obj1.free();

  // image or mask?
  dict->lookup("ImageMask", &obj1);
  if (obj1.isNull()) {
    obj1.free();
    dict->lookup("IM", &obj1);
  }
  mask = gFalse;
  if (obj1.isBool())
    mask = obj1.getBool();
  else if (!obj1.isNull())
    goto err2;
  obj1.free();

  // bit depth
  dict->lookup("BitsPerComponent", &obj1);
  if (obj1.isNull()) {
    obj1.free();
    dict->lookup("BPC", &obj1);
  }
  if (!obj1.isInt())
    goto err2;
  bits = obj1.getInt();
  obj1.free();

  // display a mask
  if (mask) {

    // check for inverted mask
    if (bits != 1)
      goto err1;
    invert = gFalse;
    dict->lookup("Decode", &obj1);
    if (obj1.isNull()) {
      obj1.free();
      dict->lookup("D", &obj1);
    }
    if (obj1.isArray()) {
      obj1.arrayGet(0, &obj2);
      if (obj2.isInt() && obj2.getInt() == 1)
	invert = gTrue;
      obj2.free();
    } else if (!obj1.isNull()) {
      goto err2;
    }
    obj1.free();

    // draw it
    out->drawImageMask(state, ref, str, width, height, invert, inlineImg);

  } else {

    // get color space and color map
    dict->lookup("ColorSpace", &obj1);
    if (obj1.isNull()) {
      obj1.free();
      dict->lookup("CS", &obj1);
    }
    if (obj1.isName()) {
      res->lookupColorSpace(obj1.getName(), &obj2);
      if (!obj2.isNull()) {
	obj1.free();
	obj1 = obj2;
      } else {
	obj2.free();
      }
    }
    colorSpace = GfxColorSpace::parse(&obj1);
    obj1.free();
    if (!colorSpace) {
      goto err1;
    }
    dict->lookup("Decode", &obj1);
    if (obj1.isNull()) {
      obj1.free();
      dict->lookup("D", &obj1);
    }
    colorMap = new GfxImageColorMap(bits, &obj1, colorSpace);
    obj1.free();
    if (!colorMap->isOk()) {
      delete colorMap;
      goto err1;
    }

    // get the mask
    haveMask = gFalse;
    dict->lookup("Mask", &maskObj);
    if (maskObj.isArray()) {
      for (i = 0; i < maskObj.arrayGetLength(); ++i) {
	maskObj.arrayGet(i, &obj1);
	maskColors[i] = obj1.getInt();
	obj1.free();
      }
      haveMask = gTrue;
    }

    // draw it
    out->drawImage(state, ref, str, width, height, colorMap,
		   haveMask ? maskColors : (int *)NULL,  inlineImg);
    delete colorMap;
    str->close();

    maskObj.free();
  }

  return;

 err2:
  obj1.free();
 err1:
  error(getPos(), "Bad image parameters");
}

void Gfx::doForm(Object *str) {
  Dict *dict;
  Object matrixObj, bboxObj;
  double m[6], bbox[6];
  Object resObj;
  Dict *resDict;
  Object obj1;
  int i;

  // get stream dict
  dict = str->streamGetDict();

  // check form type
  dict->lookup("FormType", &obj1);
  if (!(obj1.isInt() && obj1.getInt() == 1)) {
    error(getPos(), "Unknown form type");
  }
  obj1.free();

  // get bounding box
  dict->lookup("BBox", &bboxObj);
  if (!bboxObj.isArray()) {
    matrixObj.free();
    bboxObj.free();
    error(getPos(), "Bad form bounding box");
    return;
  }
  for (i = 0; i < 4; ++i) {
    bboxObj.arrayGet(i, &obj1);
    bbox[i] = obj1.getNum();
    obj1.free();
  }
  bboxObj.free();

  // get matrix
  dict->lookup("Matrix", &matrixObj);
  if (matrixObj.isArray()) {
    for (i = 0; i < 6; ++i) {
      matrixObj.arrayGet(i, &obj1);
      m[i] = obj1.getNum();
      obj1.free();
    }
  } else {
    m[0] = 1; m[1] = 0;
    m[2] = 0; m[3] = 1;
    m[4] = 0; m[5] = 0;
  }
  matrixObj.free();

  // get resources
  dict->lookup("Resources", &resObj);
  resDict = resObj.isDict() ? resObj.getDict() : (Dict *)NULL;

  // draw it
  doForm1(str, resDict, m, bbox);

  resObj.free();
}

void Gfx::doWidgetForm(Object *str, double xMin, double yMin,
		       double xMax, double yMax) {
  Dict *dict, *resDict;
  Object matrixObj, bboxObj, resObj;
  Object obj1;
  double m[6], bbox[6];
  double sx, sy;
  int i;

  // get stream dict
  dict = str->streamGetDict();

  // get bounding box
  dict->lookup("BBox", &bboxObj);
  if (!bboxObj.isArray()) {
    bboxObj.free();
    error(getPos(), "Bad form bounding box");
    return;
  }
  for (i = 0; i < 4; ++i) {
    bboxObj.arrayGet(i, &obj1);
    bbox[i] = obj1.getNum();
    obj1.free();
  }
  bboxObj.free();

  // get matrix
  dict->lookup("Matrix", &matrixObj);
  if (matrixObj.isArray()) {
    for (i = 0; i < 6; ++i) {
      matrixObj.arrayGet(i, &obj1);
      m[i] = obj1.getNum();
      obj1.free();
    }
  } else {
    m[0] = 1; m[1] = 0;
    m[2] = 0; m[3] = 1;
    m[4] = 0; m[5] = 0;
  }
  matrixObj.free();

  // scale form bbox to widget rectangle
  sx = fabs((xMax - xMin) / (bbox[2] - bbox[0]));
  sy = fabs((yMax - yMin) / (bbox[3] - bbox[1]));
  m[0] *= sx;  m[1] *= sy;
  m[2] *= sx;  m[3] *= sy;
  m[4] *= sx;  m[5] *= sy;

  // translate to widget rectangle
  m[4] += xMin;
  m[5] += yMin;

  // get resources
  dict->lookup("Resources", &resObj);
  resDict = resObj.isDict() ? resObj.getDict() : (Dict *)NULL;

  // draw it
  doForm1(str, resDict, m, bbox);

  resObj.free();
  bboxObj.free();
}

void Gfx::doForm1(Object *str, Dict *resDict, double *matrix, double *bbox) {
  Parser *oldParser;
  double oldBaseMatrix[6];
  GfxResources *resPtr;
  int i;

  // push new resources on stack
  res = new GfxResources(xref, resDict, res);

  // save current graphics state
  out->saveState(state);
  state = state->save();

  // save current parser
  oldParser = parser;

  // set form transformation matrix
  state->concatCTM(matrix[0], matrix[1], matrix[2],
		   matrix[3], matrix[4], matrix[5]);
  out->updateCTM(state, matrix[0], matrix[1], matrix[2],
		 matrix[3], matrix[4], matrix[5]);

  // set new base matrix
  for (i = 0; i < 6; ++i) {
    oldBaseMatrix[i] = baseMatrix[i];
    baseMatrix[i] = state->getCTM()[i];
  }

  // set form bounding box
  state->moveTo(bbox[0], bbox[1]);
  state->lineTo(bbox[2], bbox[1]);
  state->lineTo(bbox[2], bbox[3]);
  state->lineTo(bbox[0], bbox[3]);
  state->closePath();
  state->clip();
  out->clip(state);
  state->clearPath();

  // draw the form
  display(str, gFalse);

  // restore base matrix
  for (i = 0; i < 6; ++i) {
    baseMatrix[i] = oldBaseMatrix[i];
  }

  // restore parser
  parser = oldParser;

  // restore graphics state
  state = state->restore();
  out->restoreState(state);

  // pop resource stack
  resPtr = res->getNext();
  delete res;
  res = resPtr;

  return;
}

//------------------------------------------------------------------------
// in-line image operators
//------------------------------------------------------------------------

void Gfx::opBeginImage(Object args[], int numArgs) {
  Stream *str;
  int c1, c2;

  // build dict/stream
  str = buildImageStream();

  // display the image
  if (str) {
    doImage(NULL, str, gTrue);
  
    // skip 'EI' tag
    c1 = str->getBaseStream()->getChar();
    c2 = str->getBaseStream()->getChar();
    while (!(c1 == 'E' && c2 == 'I') && c2 != EOF) {
      c1 = c2;
      c2 = str->getBaseStream()->getChar();
    }
    delete str;
  }
}

Stream *Gfx::buildImageStream() {
  Object dict;
  Object obj;
  char *key;
  Stream *str;

  // build dictionary
  dict.initDict(xref);
  parser->getObj(&obj);
  while (!obj.isCmd("ID") && !obj.isEOF()) {
    if (!obj.isName()) {
      error(getPos(), "Inline image dictionary key must be a name object");
      obj.free();
      parser->getObj(&obj);
    } else {
      key = copyString(obj.getName());
      obj.free();
      parser->getObj(&obj);
      if (obj.isEOF() || obj.isError())
	break;
      dict.dictAdd(key, &obj);
    }
    parser->getObj(&obj);
  }
  if (obj.isEOF())
    error(getPos(), "End of file in inline image");
  obj.free();

  // make stream
  str = new EmbedStream(parser->getStream(), &dict);
  str = str->addFilters(&dict);

  return str;
}

void Gfx::opImageData(Object args[], int numArgs) {
  error(getPos(), "Internal: got 'ID' operator");
}

void Gfx::opEndImage(Object args[], int numArgs) {
  error(getPos(), "Internal: got 'EI' operator");
}

//------------------------------------------------------------------------
// type 3 font operators
//------------------------------------------------------------------------

void Gfx::opSetCharWidth(Object args[], int numArgs) {
//  error(getPos(), "Encountered 'd0' operator in content stream");
}

void Gfx::opSetCacheDevice(Object args[], int numArgs) {
//  error(getPos(), "Encountered 'd1' operator in content stream");
}

//------------------------------------------------------------------------
// compatibility operators
//------------------------------------------------------------------------

void Gfx::opBeginIgnoreUndef(Object args[], int numArgs) {
  ++ignoreUndef;
}

void Gfx::opEndIgnoreUndef(Object args[], int numArgs) {
  if (ignoreUndef > 0)
    --ignoreUndef;
}

//------------------------------------------------------------------------
// marked content operators
//------------------------------------------------------------------------

void Gfx::opBeginMarkedContent(Object args[], int numArgs) {
  if (printCommands) {
    printf("  marked content: %s ", args[0].getName());
    if (numArgs == 2)
      args[2].print(stdout);
    printf("\n");
    fflush(stdout);
  }
}

void Gfx::opEndMarkedContent(Object args[], int numArgs) {
}

void Gfx::opMarkPoint(Object args[], int numArgs) {
  if (printCommands) {
    printf("  mark point: %s ", args[0].getName());
    if (numArgs == 2)
      args[2].print(stdout);
    printf("\n");
    fflush(stdout);
  }
}
