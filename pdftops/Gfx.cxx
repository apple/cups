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
#include "Error.h"
#include "Gfx.h"

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

GBool printCommands = gFalse;

//------------------------------------------------------------------------
// Gfx
//------------------------------------------------------------------------

Gfx::Gfx(OutputDev *out1, int pageNum, Dict *resDict,
	 int dpi, double x1, double y1, double x2, double y2, GBool crop,
	 double cropX1, double cropY1, double cropX2, double cropY2,
	 int rotate) {
  Object obj1;

  // start the resource stack
  res = new GfxResources(NULL);

  // build font dictionary
  res->fonts = NULL;
  if (resDict) {
    resDict->lookup("Font", &obj1);
    if (obj1.isDict())
      res->fonts = new GfxFontDict(obj1.getDict());
    obj1.free();
  }

  // get XObject dictionary
  if (resDict)
    resDict->lookup("XObject", &res->xObjDict);
  else
    res->xObjDict.initNull();

  // get colorspace dictionary
  if (resDict)
    resDict->lookup("ColorSpace", &res->colorSpaceDict);
  else
    res->colorSpaceDict.initNull();

  // initialize
  out = out1;
  state = new GfxState(dpi, x1, y1, x2, y2, rotate, out->upsideDown());
  fontChanged = gFalse;
  clip = clipNone;
  ignoreUndef = 0;
  out->startPage(pageNum, state);
  out->setDefaultCTM(state->getCTM());
  out->updateAll(state);

  // set crop box
  if (crop) {
    state->moveTo(cropX1, cropY1);
    state->lineTo(cropX2, cropY1);
    state->lineTo(cropX2, cropY2);
    state->lineTo(cropX1, cropY2);
    state->closePath();
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
    resPtr = res->next;
    delete res;
    res = resPtr;
  }
  if (state)
    delete state;
}

GfxResources::~GfxResources() {
  if (fonts)
    delete fonts;
  xObjDict.free();
  colorSpaceDict.free();
}

void Gfx::display(Object *obj) {
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
  parser = new Parser(new Lexer(obj));
  go();
  delete parser;
  parser = NULL;
}

void Gfx::go() {
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
    }
    for (i = 0; i < numArgs; ++i)
      args[i].free();
  }

  // update display
  if (numCmds > 0)
    out->dump();

  // clean up
  if (printCommands)
    fflush(stdout);
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

GfxFont *Gfx::lookupFont(char *name) {
  GfxFont *font;
  GfxResources *resPtr;

  for (resPtr = res; resPtr; resPtr = resPtr->next) {
    if (resPtr->fonts) {
      if ((font = resPtr->fonts->lookup(name)))
	return font;
    }
  }
  error(getPos(), "unknown font tag '%s'", name);
  return NULL;
}

GBool Gfx::lookupXObject(char *name, Object *obj) {
  GfxResources *resPtr;

  for (resPtr = res; resPtr; resPtr = resPtr->next) {
    if (resPtr->xObjDict.isDict()) {
      if (!resPtr->xObjDict.dictLookup(name, obj)->isNull())
	return gTrue;
      obj->free();
    }
  }
  error(getPos(), "XObject '%s' is unknown", name);
  return gFalse;
}

void Gfx::lookupColorSpace(char *name, Object *obj) {
  GfxResources *resPtr;

  for (resPtr = res; resPtr; resPtr = resPtr->next) {
    if (resPtr->colorSpaceDict.isDict()) {
      if (!resPtr->colorSpaceDict.dictLookup(name, obj)->isNull())
	return;
      obj->free();
    }
  }
  obj->initNull();
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
}

void Gfx::opSetRenderingIntent(Object args[], int numArgs) {
}

//------------------------------------------------------------------------
// color operators
//------------------------------------------------------------------------

void Gfx::opSetFillGray(Object args[], int numArgs) {
  state->setFillColorSpace(new GfxColorSpace(colorGray));
  state->setFillGray(args[0].getNum());
  out->updateFillColor(state);
}

void Gfx::opSetStrokeGray(Object args[], int numArgs) {
  state->setStrokeColorSpace(new GfxColorSpace(colorGray));
  state->setStrokeGray(args[0].getNum());
  out->updateStrokeColor(state);
}

void Gfx::opSetFillCMYKColor(Object args[], int numArgs) {
  state->setFillColorSpace(new GfxColorSpace(colorCMYK));
  state->setFillCMYK(args[0].getNum(), args[1].getNum(),
		     args[2].getNum(), args[3].getNum());
  out->updateFillColor(state);
}

void Gfx::opSetStrokeCMYKColor(Object args[], int numArgs) {
  state->setStrokeColorSpace(new GfxColorSpace(colorCMYK));
  state->setStrokeCMYK(args[0].getNum(), args[1].getNum(),
		       args[2].getNum(), args[3].getNum());
  out->updateStrokeColor(state);
}

void Gfx::opSetFillRGBColor(Object args[], int numArgs) {
  state->setFillColorSpace(new GfxColorSpace(colorRGB));
  state->setFillRGB(args[0].getNum(), args[1].getNum(), args[2].getNum());
  out->updateFillColor(state);
}

void Gfx::opSetStrokeRGBColor(Object args[], int numArgs) {
  state->setStrokeColorSpace(new GfxColorSpace(colorRGB));
  state->setStrokeRGB(args[0].getNum(), args[1].getNum(), args[2].getNum());
  out->updateStrokeColor(state);
}

void Gfx::opSetFillColorSpace(Object args[], int numArgs) {
  Object obj;
  GfxColorSpace *colorSpace;
  double x[4];

  lookupColorSpace(args[0].getName(), &obj);
  if (obj.isNull())
    colorSpace = new GfxColorSpace(&args[0]);
  else
    colorSpace = new GfxColorSpace(&obj);
  obj.free();
  if (colorSpace->isOk()) {
    state->setFillColorSpace(colorSpace);
  } else {
    delete colorSpace;
    error(getPos(), "Bad colorspace");
  }
  x[0] = x[1] = x[2] = x[3] = 0;
  state->setFillColor(x);
  out->updateFillColor(state);
}

void Gfx::opSetStrokeColorSpace(Object args[], int numArgs) {
  Object obj;
  GfxColorSpace *colorSpace;
  double x[4];

  lookupColorSpace(args[0].getName(), &obj);
  if (obj.isNull())
    colorSpace = new GfxColorSpace(&args[0]);
  else
    colorSpace = new GfxColorSpace(&obj);
  obj.free();
  if (colorSpace->isOk()) {
    state->setStrokeColorSpace(colorSpace);
  } else {
    delete colorSpace;
    error(getPos(), "Bad colorspace");
  }
  x[0] = x[1] = x[2] = x[3] = 0;
  state->setStrokeColor(x);
  out->updateStrokeColor(state);
}

void Gfx::opSetFillColor(Object args[], int numArgs) {
  double x[4];
  int i;

  x[0] = x[1] = x[2] = x[3] = 0;
  for (i = 0; i < numArgs; ++i)
    x[i] = args[i].getNum();
  state->setFillColor(x);
  out->updateFillColor(state);
}

void Gfx::opSetStrokeColor(Object args[], int numArgs) {
  double x[4];
  int i;

  x[0] = x[1] = x[2] = x[3] = 0;
  for (i = 0; i < numArgs; ++i)
    x[i] = args[i].getNum();
  state->setStrokeColor(x);
  out->updateStrokeColor(state);
}

void Gfx::opSetFillColorN(Object args[], int numArgs) {
  double x[4];
  int i;

  x[0] = x[1] = x[2] = x[3] = 0;
  for (i = 0; i < numArgs && i < 4; ++i) {
    if (args[i].isNum())
      x[i] = args[i].getNum();
    else
      break;
  }
  state->setFillColor(x);
  out->updateFillColor(state);
}

void Gfx::opSetStrokeColorN(Object args[], int numArgs) {
  double x[4];
  int i;

  x[0] = x[1] = x[2] = x[3] = 0;
  for (i = 0; i < numArgs && i < 4; ++i) {
    if (args[i].isNum())
      x[i] = args[i].getNum();
    else
      break;
  }
  state->setStrokeColor(x);
  out->updateStrokeColor(state);
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
  if (state->isPath())
    out->fill(state);
  doEndPath();
}

void Gfx::opEOFill(Object args[], int numArgs) {
  if (!state->isCurPt()) {
    //error(getPos(), "No path in eofill");
    return;
  }
  if (state->isPath())
    out->eoFill(state);
  doEndPath();
}

void Gfx::opFillStroke(Object args[], int numArgs) {
  if (!state->isCurPt()) {
    //error(getPos(), "No path in fill/stroke");
    return;
  }
  if (state->isPath()) {
    out->fill(state);
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
    out->fill(state);
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
    out->eoFill(state);
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
    out->eoFill(state);
    out->stroke(state);
  }
  doEndPath();
}

void Gfx::opShFill(Object args[], int numArgs) {
}

void Gfx::doEndPath() {
  if (state->isPath()) {
    if (clip == clipNormal)
      out->clip(state);
    else if (clip == clipEO)
      out->eoClip(state);
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

  if (!(font = lookupFont(args[0].getName())))
    return;
  if (printCommands) {
    printf("  font: '%s' %g\n",
	   font->getName() ? font->getName()->getCString() : "???",
	   args[1].getNum());
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
#if 0 //~type3
  double dx, dy, width, height, w, h, x, y;
  double oldCTM[6], newCTM[6];
  double *mat;
  Object charProc;
  Parser *oldParser;
  int i;
#else
  double dx, dy, width, height, w, h, sWidth, sHeight;
#endif

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
	width = state->getFontSize() * state->getHorizScaling() *
	        font->getWidth16(c16) +
	        state->getCharSpace();
	if (c16 == ' ') {
	  width += state->getWordSpace();
	}
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
#if 0 //~type3
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
	state->setCTM(newCTM[0], newCTM[1], newCTM[2], newCTM[3], x, y);
	//~ out->updateCTM(???)
	if (charProc.isStream()) {
	  display(&charProc);
	} else {
	  error(getPos(), "Missing or bad Type3 CharProc entry");
	}
	state->setCTM(oldCTM[0], oldCTM[1], oldCTM[2],
		      oldCTM[3], oldCTM[4], oldCTM[5]);
	//~ out->updateCTM(???) - use gsave/grestore instead?
	charProc.free();
	width = state->getFontSize() * state->getHorizScaling() *
	        font->getWidth(c8) +
	        state->getCharSpace();
	if (c8 == ' ') {
	  width += state->getWordSpace();
	}
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
	width = state->getFontSize() * state->getHorizScaling() *
	        font->getWidth(c8) +
	        state->getCharSpace();
	if (c8 == ' ')
	  width += state->getWordSpace();
	state->textTransformDelta(width, 0, &w, &h);
	out->drawChar(state, state->getCurX() + dx, state->getCurY() + dy,
		      w, h, c8);
	state->textShift(width);
      }
      out->endString(state);
    } else {
      out->drawString(state, s);
      width = state->getFontSize() * state->getHorizScaling() *
	      font->getWidth(s) +
	      s->getLength() * state->getCharSpace();
      for (p = (Guchar *)s->getCString(), n = s->getLength(); n; ++p, --n) {
	if (*p == ' ')
	  width += state->getWordSpace();
      }
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
  Object obj1, obj2;
#if OPI_SUPPORT
  Object opiDict;
#endif

  if (!lookupXObject(args[0].getName(), &obj1))
    return;
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
  if (obj2.isName("Image"))
    doImage(obj1.getStream(), gFalse);
  else if (obj2.isName("Form"))
    doForm(&obj1);
  else if (obj2.isName())
    error(getPos(), "Unknown XObject subtype '%s'", obj2.getName());
  else
    error(getPos(), "XObject subtype is missing or wrong type");
  obj2.free();
#if OPI_SUPPORT
  if (opiDict.isDict()) {
    out->opiEnd(state, opiDict.getDict());
  }
  opiDict.free();
#endif
  obj1.free();
}

void Gfx::doImage(Stream *str, GBool inlineImg) {
  Dict *dict;
  Object obj1, obj2;
  int width, height;
  int bits;
  GBool mask;
  GfxColorSpace *colorSpace;
  GfxImageColorMap *colorMap;
  GBool invert;

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
    out->drawImageMask(state, str, width, height, invert, inlineImg);

  } else {

    // get color space and color map
    dict->lookup("ColorSpace", &obj1);
    if (obj1.isNull()) {
      obj1.free();
      dict->lookup("CS", &obj1);
    }
    if (obj1.isName()) {
      lookupColorSpace(obj1.getName(), &obj2);
      if (!obj2.isNull()) {
	obj1.free();
	obj1 = obj2;
      } else {
	obj2.free();
      }
    }
    colorSpace = new GfxColorSpace(&obj1);
    obj1.free();
    if (!colorSpace->isOk()) {
      delete colorSpace;
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
      delete colorSpace;
      goto err1;
    }

    // draw it
    out->drawImage(state, str, width, height, colorMap, inlineImg);
    delete colorMap;
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
  Object obj1;

  // get stream dict
  dict = str->streamGetDict();

  // check form type
  dict->lookup("FormType", &obj1);
  if (!(obj1.isInt() && obj1.getInt() == 1)) {
    error(getPos(), "Unknown form type");
  }
  obj1.free();

  // get matrix and bounding box
  dict->lookup("Matrix", &matrixObj);
  if (!matrixObj.isArray()) {
    matrixObj.free();
    error(getPos(), "Bad form matrix");
    return;
  }
  dict->lookup("BBox", &bboxObj);
  if (!bboxObj.isArray()) {
    matrixObj.free();
    bboxObj.free();
    error(getPos(), "Bad form bounding box");
    return;
  }

  doForm1(str, dict, &matrixObj, &bboxObj);

  matrixObj.free();
  bboxObj.free();
}

void Gfx::doWidgetForm(Object *str, double x, double y) {
  Dict *dict;
  Object matrixObj, bboxObj;
  Object obj1;

  // get stream dict
  dict = str->streamGetDict();

  // get bounding box
  dict->lookup("BBox", &bboxObj);
  if (!bboxObj.isArray()) {
    bboxObj.free();
    error(getPos(), "Bad form bounding box");
    return;
  }

  // construct matrix
  matrixObj.initArray();
  obj1.initReal(1);
  matrixObj.arrayAdd(&obj1);
  obj1.initReal(0);
  matrixObj.arrayAdd(&obj1);
  obj1.initReal(0);
  matrixObj.arrayAdd(&obj1);
  obj1.initReal(1);
  matrixObj.arrayAdd(&obj1);
  obj1.initReal(x);
  matrixObj.arrayAdd(&obj1);
  obj1.initReal(y);
  matrixObj.arrayAdd(&obj1);

  doForm1(str, dict, &matrixObj, &bboxObj);

  matrixObj.free();
  bboxObj.free();
}

void Gfx::doForm1(Object *str, Dict *dict,
		  Object *matrixObj, Object *bboxObj) {
  Parser *oldParser;
  GfxResources *resPtr;
  Dict *resDict;
  double m[6];
  Object obj1, obj2;
  int i;

  // push new resources on stack
  res = new GfxResources(res);
  dict->lookup("Resources", &obj1);
  if (obj1.isDict()) {
    resDict = obj1.getDict();
    res->fonts = NULL;
    resDict->lookup("Font", &obj2);
    if (obj2.isDict())
      res->fonts = new GfxFontDict(obj2.getDict());
    obj2.free();
    resDict->lookup("XObject", &res->xObjDict);
    resDict->lookup("ColorSpace", &res->colorSpaceDict);
    obj1.free();
  }

  // save current graphics state
  out->saveState(state);
  state = state->save();

  // save current parser
  oldParser = parser;

  // set form transformation matrix
  for (i = 0; i < 6; ++i) {
    matrixObj->arrayGet(i, &obj1);
    m[i] = obj1.getNum();
    obj1.free();
  }
  state->concatCTM(m[0], m[1], m[2], m[3], m[4], m[5]);
  out->updateCTM(state, m[0], m[1], m[2], m[3], m[4], m[5]);

  // set form bounding box
  for (i = 0; i < 4; ++i) {
    bboxObj->arrayGet(i, &obj1);
    m[i] = obj1.getNum();
    obj1.free();
  }
  state->moveTo(m[0], m[1]);
  state->lineTo(m[2], m[1]);
  state->lineTo(m[2], m[3]);
  state->lineTo(m[0], m[3]);
  state->closePath();
  out->clip(state);
  state->clearPath();

  // draw the form
  display(str);

  // restore parser
  parser = oldParser;

  // restore graphics state
  state = state->restore();
  out->restoreState(state);

  // pop resource stack
  resPtr = res->next;
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
    doImage(str, gTrue);
  
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
  dict.initDict();
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
  error(getPos(), "Encountered 'd0' operator in content stream");
}

void Gfx::opSetCacheDevice(Object args[], int numArgs) {
  error(getPos(), "Encountered 'd1' operator in content stream");
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
  }
}
