//========================================================================
//
// Annot.h
//
// Copyright 2000-2003 Glyph & Cog, LLC
//
//========================================================================

#ifndef ANNOT_H
#define ANNOT_H

#include <config.h>

#ifdef USE_GCC_PRAGMAS
#pragma interface
#endif

class XRef;
class Catalog;
class Gfx;

//------------------------------------------------------------------------
// Annot
//------------------------------------------------------------------------

class Annot {
public:

  Annot(XRef *xrefA, Dict *acroForm, Dict *dict);
  ~Annot();
  GBool isOk() { return ok; }

  void draw(Gfx *gfx);

  // Get appearance object.
  Object *getAppearance(Object *obj) { return appearance.fetch(xref, obj); }

private:
 
  void generateAppearance(Dict *acroForm, Dict *dict);

  XRef *xref;			// the xref table for this PDF file
  Object appearance;		// a reference to the Form XObject stream
				//   for the normal appearance
  GString *appearBuf;
  double xMin, yMin,		// annotation rectangle
         xMax, yMax;
  GBool ok;
};

//------------------------------------------------------------------------
// Annots
//------------------------------------------------------------------------

class Annots {
public:

  // Extract non-link annotations from array of annotations.
  Annots(XRef *xref, Catalog *catalog, Object *annotsObj);

  ~Annots();

  // Iterate through list of annotations.
  int getNumAnnots() { return nAnnots; }
  Annot *getAnnot(int i) { return annots[i]; }

private:

  Annot **annots;
  int nAnnots;
};

#endif
