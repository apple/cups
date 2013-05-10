//========================================================================
//
// Page.h
//
// Copyright 1996 Derek B. Noonburg
//
//========================================================================

#ifndef PAGE_H
#define PAGE_H

#ifdef __GNUC__
#pragma interface
#endif

#include "Object.h"

class Dict;
class XRef;
class OutputDev;
class Links;
class Catalog;

//------------------------------------------------------------------------
// PageAttrs
//------------------------------------------------------------------------

class PageAttrs {
public:

  // Construct a new PageAttrs object by merging a dictionary
  // (of type Pages or Page) into another PageAttrs object.  If
  // <attrs> is NULL, uses defaults.
  PageAttrs(PageAttrs *attrs, Dict *dict);

  // Destructor.
  ~PageAttrs();

  // Accessors.
  double getX1() { return limitToCropBox ? cropX1 : x1; }
  double getY1() { return limitToCropBox ? cropY1 : y1; }
  double getX2() { return limitToCropBox ? cropX2 : x2; }
  double getY2() { return limitToCropBox ? cropY2 : y2; }
  GBool isCropped() { return cropX2 > cropX1; }
  double getCropX1() { return cropX1; }
  double getCropY1() { return cropY1; }
  double getCropX2() { return cropX2; }
  double getCropY2() { return cropY2; }
  int getRotate() { return rotate; }
  Dict *getResourceDict()
    { return resources.isDict() ? resources.getDict() : (Dict *)NULL; }

private:

  double x1, y1, x2, y2;
  double cropX1, cropY1, cropX2, cropY2;
  GBool limitToCropBox;
  int rotate;
  Object resources;
};

//------------------------------------------------------------------------
// Page
//------------------------------------------------------------------------

class Page {
public:

  // Constructor.
  Page(int num1, Dict *pageDict, PageAttrs *attrs1);

  // Destructor.
  ~Page();

  // Is page valid?
  GBool isOk() { return ok; }

  // Get page parameters.
  double getX1() { return attrs->getX1(); }
  double getY1() { return attrs->getY1(); }
  double getX2() { return attrs->getX2(); }
  double getY2() { return attrs->getY2(); }
  GBool isCropped() { return attrs->isCropped(); }
  double getCropX1() { return attrs->getCropX1(); }
  double getCropY1() { return attrs->getCropY1(); }
  double getCropX2() { return attrs->getCropX2(); }
  double getCropY2() { return attrs->getCropY2(); }
  double getWidth() { return attrs->getX2() - attrs->getX1(); }
  double getHeight() { return attrs->getY2() - attrs->getY1(); }
  int getRotate() { return attrs->getRotate(); }

  // Get resource dictionary.
  Dict *getResourceDict() { return attrs->getResourceDict(); }

  // Get annotations array.
  Object *getAnnots(Object *obj) { return annots.fetch(obj); }

  // Get contents.
  Object *getContents(Object *obj) { return contents.fetch(obj); }

  // Display a page.
  void display(OutputDev *out, double dpi, int rotate,
	       Links *links, Catalog *catalog);

private:

  int num;			// page number
  PageAttrs *attrs;		// page attributes
  Object annots;		// annotations array
  Object contents;		// page contents
  GBool ok;			// true if page is valid
};

#endif
