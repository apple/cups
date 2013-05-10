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

struct PDFRectangle {
  double x1, y1, x2, y2;
};

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
  PDFRectangle *getBox() { return limitToCropBox ? &cropBox : &mediaBox; }
  PDFRectangle *getMediaBox() { return &mediaBox; }
  PDFRectangle *getCropBox() { return &cropBox; }
  GBool isCropped() { return haveCropBox; }
  PDFRectangle *getBleedBox() { return &bleedBox; }
  PDFRectangle *getTrimBox() { return &trimBox; }
  PDFRectangle *getArtBox() { return &artBox; }
  int getRotate() { return rotate; }
  Dict *getResourceDict()
    { return resources.isDict() ? resources.getDict() : (Dict *)NULL; }

private:

  GBool readBox(Dict *dict, char *key, PDFRectangle *box);

  PDFRectangle mediaBox;
  PDFRectangle cropBox;
  GBool haveCropBox;
  GBool limitToCropBox;
  PDFRectangle bleedBox;
  PDFRectangle trimBox;
  PDFRectangle artBox;
  int rotate;
  Object resources;
};

//------------------------------------------------------------------------
// Page
//------------------------------------------------------------------------

class Page {
public:

  // Constructor.
  Page(XRef *xrefA, int numA, Dict *pageDict, PageAttrs *attrsA,
       GBool printCommandsA);

  // Destructor.
  ~Page();

  // Is page valid?
  GBool isOk() { return ok; }

  // Get page parameters.
  PDFRectangle *getBox() { return attrs->getBox(); }
  PDFRectangle *getMediaBox() { return attrs->getMediaBox(); }
  PDFRectangle *getCropBox() { return attrs->getCropBox(); }
  GBool isCropped() { return attrs->isCropped(); }
  double getWidth() { return attrs->getBox()->x2 - attrs->getBox()->x1; }
  double getHeight() { return attrs->getBox()->y2 - attrs->getBox()->y1; }
  PDFRectangle *getBleedBox() { return attrs->getBleedBox(); }
  PDFRectangle *getTrimBox() { return attrs->getTrimBox(); }
  PDFRectangle *getArtBox() { return attrs->getArtBox(); }
  int getRotate() { return attrs->getRotate(); }

  // Get resource dictionary.
  Dict *getResourceDict() { return attrs->getResourceDict(); }

  // Get annotations array.
  Object *getAnnots(Object *obj) { return annots.fetch(xref, obj); }

  // Get contents.
  Object *getContents(Object *obj) { return contents.fetch(xref, obj); }

  // Display a page.
  void display(OutputDev *out, double dpi, int rotate,
	       Links *links, Catalog *catalog);

private:

  XRef *xref;			// the xref table for this PDF file
  int num;			// page number
  PageAttrs *attrs;		// page attributes
  Object annots;		// annotations array
  Object contents;		// page contents
  GBool printCommands;		// print the drawing commands (for debugging)
  GBool ok;			// true if page is valid
};

#endif
