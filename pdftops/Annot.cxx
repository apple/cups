//========================================================================
//
// Annot.cc
//
// Copyright 2000-2002 Glyph & Cog, LLC
//
//========================================================================

#include <config.h>

#ifdef USE_GCC_PRAGMAS
#pragma implementation
#endif

#include "gmem.h"
#include "Object.h"
#include "Gfx.h"
#include "Annot.h"

//------------------------------------------------------------------------
// Annot
//------------------------------------------------------------------------

Annot::Annot(XRef *xrefA, Dict *dict) {
  Object apObj, asObj, obj1, obj2;
  double t;

  ok = gFalse;
  xref = xrefA;

  if (dict->lookup("AP", &apObj)->isDict()) {
    if (dict->lookup("AS", &asObj)->isName()) {
      if (apObj.dictLookup("N", &obj1)->isDict()) {
	if (obj1.dictLookupNF(asObj.getName(), &obj2)->isRef()) {
	  obj2.copy(&appearance);
	  ok = gTrue;
	}
	obj2.free();
      }
      obj1.free();
    } else {
      if (apObj.dictLookupNF("N", &obj1)->isRef()) {
	obj1.copy(&appearance);
	ok = gTrue;
      }
      obj1.free();
    }
    asObj.free();
  }
  apObj.free();

  if (dict->lookup("Rect", &obj1)->isArray() &&
      obj1.arrayGetLength() == 4) {
    //~ should check object types here
    obj1.arrayGet(0, &obj2);
    xMin = obj2.getNum();
    obj2.free();
    obj1.arrayGet(1, &obj2);
    yMin = obj2.getNum();
    obj2.free();
    obj1.arrayGet(2, &obj2);
    xMax = obj2.getNum();
    obj2.free();
    obj1.arrayGet(3, &obj2);
    yMax = obj2.getNum();
    obj2.free();
    if (xMin > xMax) {
      t = xMin; xMin = xMax; xMax = t;
    }
    if (yMin > yMax) {
      t = yMin; yMin = yMax; yMax = t;
    }
  } else {
    //~ this should return an error
    xMin = yMin = 0;
    xMax = yMax = 1;
  }
  obj1.free();
}

Annot::~Annot() {
  appearance.free();
}

void Annot::draw(Gfx *gfx) {
  Object obj;

  if (appearance.fetch(xref, &obj)->isStream()) {
    gfx->doAnnot(&obj, xMin, yMin, xMax, yMax);
  }
  obj.free();
}

//------------------------------------------------------------------------
// Annots
//------------------------------------------------------------------------

Annots::Annots(XRef *xref, Object *annotsObj) {
  Annot *annot;
  Object obj1, obj2;
  int size;
  int i;

  annots = NULL;
  size = 0;
  nAnnots = 0;

  if (annotsObj->isArray()) {
    for (i = 0; i < annotsObj->arrayGetLength(); ++i) {
      if (annotsObj->arrayGet(i, &obj1)->isDict()) {
	obj1.dictLookup("Subtype", &obj2);
	if (obj2.isName("Widget") ||
	    obj2.isName("Stamp")) {
	  annot = new Annot(xref, obj1.getDict());
	  if (annot->isOk()) {
	    if (nAnnots >= size) {
	      size += 16;
	      annots = (Annot **)grealloc(annots, size * sizeof(Annot *));
	    }
	    annots[nAnnots++] = annot;
	  } else {
	    delete annot;
	  }
	}
	obj2.free();
      }
      obj1.free();
    }
  }
}

Annots::~Annots() {
  int i;

  for (i = 0; i < nAnnots; ++i) {
    delete annots[i];
  }
  gfree(annots);
}
