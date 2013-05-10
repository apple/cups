//========================================================================
//
// PDFDoc.h
//
// Copyright 1996-2002 Glyph & Cog, LLC
//
//========================================================================

#ifndef PDFDOC_H
#define PDFDOC_H

#include <config.h>

#ifdef USE_GCC_PRAGMAS
#pragma interface
#endif

#include <stdio.h>
#include "XRef.h"
#include "Link.h"
#include "Catalog.h"
#include "Page.h"

class GString;
class BaseStream;
class OutputDev;
class Links;
class LinkAction;
class LinkDest;
class Outline;

//------------------------------------------------------------------------
// PDFDoc
//------------------------------------------------------------------------

class PDFDoc {
public:

  PDFDoc(GString *fileNameA, GString *ownerPassword = NULL,
	 GString *userPassword = NULL);
  PDFDoc(BaseStream *strA, GString *ownerPassword = NULL,
	 GString *userPassword = NULL);
  ~PDFDoc();

  // Was PDF document successfully opened?
  GBool isOk() { return ok; }

  // Get the error code (if isOk() returns false).
  int getErrorCode() { return errCode; }

  // Get file name.
  GString *getFileName() { return fileName; }

  // Get the xref table.
  XRef *getXRef() { return xref; }

  // Get catalog.
  Catalog *getCatalog() { return catalog; }

  // Get base stream.
  BaseStream *getBaseStream() { return str; }

  // Get page parameters.
  double getPageWidth(int page)
    { return catalog->getPage(page)->getWidth(); }
  double getPageHeight(int page)
    { return catalog->getPage(page)->getHeight(); }
  int getPageRotate(int page)
    { return catalog->getPage(page)->getRotate(); }

  // Get number of pages.
  int getNumPages() { return catalog->getNumPages(); }

  // Return the contents of the metadata stream, or NULL if there is
  // no metadata.
  GString *readMetadata() { return catalog->readMetadata(); }

  // Return the structure tree root object.
  Object *getStructTreeRoot() { return catalog->getStructTreeRoot(); }

  // Display a page.
  void displayPage(OutputDev *out, int page, double zoom,
		   int rotate, GBool doLinks,
		   GBool (*abortCheckCbk)(void *data) = NULL,
		   void *abortCheckCbkData = NULL);

  // Display a range of pages.
  void displayPages(OutputDev *out, int firstPage, int lastPage,
		    int zoom, int rotate, GBool doLinks,
		    GBool (*abortCheckCbk)(void *data) = NULL,
		    void *abortCheckCbkData = NULL);

  // Display part of a page.
  void displayPageSlice(OutputDev *out, int page, double zoom,
			int rotate, int sliceX, int sliceY,
			int sliceW, int sliceH,
			GBool (*abortCheckCbk)(void *data) = NULL,
			void *abortCheckCbkData = NULL);

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

#ifndef DISABLE_OUTLINE
  // Return the outline object.
  Outline *getOutline() { return outline; }
#endif

  // Is the file encrypted?
  GBool isEncrypted() { return xref->isEncrypted(); }

  // Check various permissions.
  GBool okToPrint(GBool ignoreOwnerPW = gFalse)
    { return xref->okToPrint(ignoreOwnerPW); }
  GBool okToChange(GBool ignoreOwnerPW = gFalse)
    { return xref->okToChange(ignoreOwnerPW); }
  GBool okToCopy(GBool ignoreOwnerPW = gFalse)
    { return xref->okToCopy(ignoreOwnerPW); }
  GBool okToAddNotes(GBool ignoreOwnerPW = gFalse)
    { return xref->okToAddNotes(ignoreOwnerPW); }

  // Is this document linearized?
  GBool isLinearized();

  // Return the document's Info dictionary (if any).
  Object *getDocInfo(Object *obj) { return xref->getDocInfo(obj); }
  Object *getDocInfoNF(Object *obj) { return xref->getDocInfoNF(obj); }

  // Return the PDF version specified by the file.
  double getPDFVersion() { return pdfVersion; }

  // Save this file with another name.
  GBool saveAs(GString *name);


private:

  GBool setup(GString *ownerPassword, GString *userPassword);
  void checkHeader();
  void getLinks(Page *page);

  GString *fileName;
  FILE *file;
  BaseStream *str;
  double pdfVersion;
  XRef *xref;
  Catalog *catalog;
  Links *links;
#ifndef DISABLE_OUTLINE
  Outline *outline;
#endif


  GBool ok;
  int errCode;
};

#endif
