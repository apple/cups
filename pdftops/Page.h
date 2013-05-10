//========================================================================
//
// Page.h
//
// Copyright 1996-2002 Glyph & Cog, LLC
//
//========================================================================

#ifndef PAGE_H
#define PAGE_H

#include <config.h>

#ifdef USE_GCC_PRAGMAS
#pragma interface
#endif

#include "Object.h"

class Dict;
class XRef;
class OutputDev;
class Links;
class Catalog;

//------------------------------------------------------------------------

class PDFRectangle {
public:
  double x1, y1, x2, y2;

  PDFRectangle() { x1 = y1 = x2 = y2 = 0; }
  PDFRectangle(double x1A, double y1A, double x2A, double y2A)
    { x1 = x1A; y1 = y1A; x2 = x2A; y2 = y2A; }
  GBool isValid() { return x1 != 0 || y1 != 0 || x2 != 0 || y2 != 0; }
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
  GString *getLastModified()
    { return lastModified.isString()
	? lastModified.getString() : (GString *)NULL; }
  Dict *getBoxColorInfo()
    { return boxColorInfo.isDict() ? boxColorInfo.getDict() : (Dict *)NULL; }
  Dict *getGroup()
    { return group.isDict() ? group.getDict() : (Dict *)NULL; }
  Stream *getMetadata()
    { return metadata.isStream() ? metadata.getStream() : (Stream *)NULL; }
  Dict *getPieceInfo()
    { return pieceInfo.isDict() ? pieceInfo.getDict() : (Dict *)NULL; }
  Dict *getSeparationInfo()
    { return separationInfo.isDict()
	? separationInfo.getDict() : (Dict *)NULL; }
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
  Object lastModified;
  Object boxColorInfo;
  Object group;
  Object metadata;
  Object pieceInfo;
  Object separationInfo;
  Object resources;
};

//------------------------------------------------------------------------
// Page
//------------------------------------------------------------------------

class Page {
public:

  // Constructor.
  Page(XRef *xrefA, int numA, Dict *pageDict, PageAttrs *attrsA);

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
  GString *getLastModified() { return attrs->getLastModified(); }
  Dict *getBoxColorInfo() { return attrs->getBoxColorInfo(); }
  Dict *getGroup() { return attrs->getGroup(); }
  Stream *getMetadata() { return attrs->getMetadata(); }
  Dict *getPieceInfo() { return attrs->getPieceInfo(); }
  Dict *getSeparationInfo() { return attrs->getSeparationInfo(); }

  // Get resource dictionary.
  Dict *getResourceDict() { return attrs->getResourceDict(); }

  // Get annotations array.
  Object *getAnnots(Object *obj) { return annots.fetch(xref, obj); }

  // Get contents.
  Object *getContents(Object *obj) { return contents.fetch(xref, obj); }

  // Display a page.
  void display(OutputDev *out, double dpi, int rotate,
	       Links *links, Catalog *catalog,
	       GBool (*abortCheckCbk)(void *data) = NULL,
	       void *abortCheckCbkData = NULL);

  // Display part of a page.
  void displaySlice(OutputDev *out, double dpi, int rotate,
		    int sliceX, int sliceY, int sliceW, int sliceH,
		    Links *links, Catalog *catalog,
		    GBool (*abortCheckCbk)(void *data) = NULL,
		    void *abortCheckCbkData = NULL);

private:

  XRef *xref;			// the xref table for this PDF file
  int num;			// page number
  PageAttrs *attrs;		// page attributes
  Object annots;		// annotations array
  Object contents;		// page contents
  GBool ok;			// true if page is valid
};

#endif
