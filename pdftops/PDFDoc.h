//========================================================================
//
// PDFDoc.h
//
// Copyright 1996 Derek B. Noonburg
//
//========================================================================

#ifndef PDFDOC_H
#define PDFDOC_H

#ifdef __GNUC__
#pragma interface
#endif

#include <stdio.h>
#include "Link.h"

class GString;
class XRef;
class Catalog;
class OutputDev;
class Links;
class LinkAction;
class LinkDest;

//------------------------------------------------------------------------
// PDFDoc
//------------------------------------------------------------------------

class PDFDoc {
public:

  PDFDoc(GString *fileName1);
  ~PDFDoc();

  // Was PDF document successfully opened?
  GBool isOk() { return ok; }

  // Get file name.
  GString *getFileName() { return fileName; }

  // Get catalog.
  Catalog *getCatalog() { return catalog; }

  // Get page parameters.
  double getPageWidth(int page)
    { return catalog->getPage(page)->getWidth(); }
  double getPageHeight(int page)
    { return catalog->getPage(page)->getHeight(); }
  int getPageRotate(int page)
    { return catalog->getPage(page)->getRotate(); }

  // Get number of pages.
  int getNumPages() { return catalog->getNumPages(); }

  // Display a page.
  void displayPage(OutputDev *out, int page, int zoom, int rotate,
		   GBool doLinks);

  // Display a range of pages.
  void displayPages(OutputDev *out, int firstPage, int lastPage,
		    int zoom, int rotate);

  // Find a page, given its object ID.  Returns page number, or 0 if
  // not found.
  int findPage(int num, int gen) { return catalog->findPage(num, gen); }

  // If point <x>,<y> is in a link, return the associated action;
  // else return NULL.
  LinkAction *findLink(double x, double y) { return links->find(x, y); }

  // Return true if <x>,<y> is in a link.
  GBool onLink(double x, double y) { return links->onLink(x, y); }

  // Find a named destination.  Returns the link destination, or
  // NULL if <name> is not a destination.
  LinkDest *findDest(GString *name)
    { return catalog->findDest(name); }

  // Is the file encrypted?
  GBool isEncrypted() { return xref->isEncrypted(); }

  // Are printing and copying allowed?  If not, print an error message.
  GBool okToPrint() { return xref->okToPrint(); }
  GBool okToCopy() { return xref->okToCopy(); }

  // Return the document's Info dictionary (if any).
  Object *getDocInfo(Object *obj) { return xref->getDocInfo(obj); }

  // Save this file with another name.
  GBool saveAs(GString *name);

private:

  void getLinks(int page);

  GString *fileName;
  FILE *file;
  XRef *xref;
  Catalog *catalog;
  Links *links;

  GBool ok;
};

#endif
