//========================================================================
//
// PDFDoc.cc
//
// Copyright 1996-2003 Glyph & Cog, LLC
//
//========================================================================

#include <config.h>

#ifdef USE_GCC_PRAGMAS
#pragma implementation
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include "GString.h"
#include "config.h"
#include "GlobalParams.h"
#include "Page.h"
#include "Catalog.h"
#include "Stream.h"
#include "XRef.h"
#include "Link.h"
#include "OutputDev.h"
#include "Error.h"
#include "ErrorCodes.h"
#include "Lexer.h"
#include "Parser.h"
#ifndef DISABLE_OUTLINE
#include "Outline.h"
#endif
#include "PDFDoc.h"

//------------------------------------------------------------------------

#define headerSearchSize 1024	// read this many bytes at beginning of
				//   file to look for '%PDF'

//------------------------------------------------------------------------
// PDFDoc
//------------------------------------------------------------------------

PDFDoc::PDFDoc(GString *fileNameA, GString *ownerPassword,
	       GString *userPassword) {
  Object obj;
  GString *fileName1, *fileName2;

  ok = gFalse;
  errCode = errNone;

  file = NULL;
  str = NULL;
  xref = NULL;
  catalog = NULL;
  links = NULL;
#ifndef DISABLE_OUTLINE
  outline = NULL;
#endif

  fileName = fileNameA;
  fileName1 = fileName;


  // try to open file
  fileName2 = NULL;
#ifdef VMS
  if (!(file = fopen(fileName1->getCString(), "rb", "ctx=stm"))) {
    error(-1, "Couldn't open file '%s'", fileName1->getCString());
    errCode = errOpenFile;
    return;
  }
#else
  if (!(file = fopen(fileName1->getCString(), "rb"))) {
    fileName2 = fileName->copy();
    fileName2->lowerCase();
    if (!(file = fopen(fileName2->getCString(), "rb"))) {
      fileName2->upperCase();
      if (!(file = fopen(fileName2->getCString(), "rb"))) {
	error(-1, "Couldn't open file '%s'", fileName->getCString());
	delete fileName2;
	errCode = errOpenFile;
	return;
      }
    }
    delete fileName2;
  }
#endif

  // create stream
  obj.initNull();
  str = new FileStream(file, 0, gFalse, 0, &obj);

  ok = setup(ownerPassword, userPassword);
}

PDFDoc::PDFDoc(BaseStream *strA, GString *ownerPassword,
	       GString *userPassword) {
  ok = gFalse;
  errCode = errNone;
  fileName = NULL;
  file = NULL;
  str = strA;
  xref = NULL;
  catalog = NULL;
  links = NULL;
#ifndef DISABLE_OUTLINE
  outline = NULL;
#endif
  ok = setup(ownerPassword, userPassword);
}

GBool PDFDoc::setup(GString *ownerPassword, GString *userPassword) {
  // check header
  checkHeader();

  // read xref table
  xref = new XRef(str, ownerPassword, userPassword);
  if (!xref->isOk()) {
    error(-1, "Couldn't read xref table");
    errCode = xref->getErrorCode();
    return gFalse;
  }

  // read catalog
  catalog = new Catalog(xref);
  if (!catalog->isOk()) {
    error(-1, "Couldn't read page catalog");
    errCode = errBadCatalog;
    return gFalse;
  }

#ifndef DISABLE_OUTLINE
  // read outline
  outline = new Outline(catalog->getOutline(), xref);
#endif

  // done
  return gTrue;
}

PDFDoc::~PDFDoc() {
#ifndef DISABLE_OUTLINE
  if (outline) {
    delete outline;
  }
#endif
  if (catalog) {
    delete catalog;
  }
  if (xref) {
    delete xref;
  }
  if (str) {
    delete str;
  }
  if (file) {
    fclose(file);
  }
  if (fileName) {
    delete fileName;
  }
  if (links) {
    delete links;
  }
}

// Check for a PDF header on this stream.  Skip past some garbage
// if necessary.
void PDFDoc::checkHeader() {
  char hdrBuf[headerSearchSize+1];
  char *p;
  int i;

  pdfVersion = 0;
  for (i = 0; i < headerSearchSize; ++i) {
    hdrBuf[i] = str->getChar();
  }
  hdrBuf[headerSearchSize] = '\0';
  for (i = 0; i < headerSearchSize - 5; ++i) {
    if (!strncmp(&hdrBuf[i], "%PDF-", 5)) {
      break;
    }
  }
  if (i >= headerSearchSize - 5) {
    error(-1, "May not be a PDF file (continuing anyway)");
    return;
  }
  str->moveStart(i);
  p = strtok(&hdrBuf[i+5], " \t\n\r");
  pdfVersion = atof(p);
  if (!(hdrBuf[i+5] >= '0' && hdrBuf[i+5] <= '9') ||
      pdfVersion > supportedPDFVersionNum + 0.0001) {
    error(-1, "PDF version %s -- xpdf supports version %s"
	  " (continuing anyway)", p, supportedPDFVersionStr);
  }
}

void PDFDoc::displayPage(OutputDev *out, int page, double zoom,
			 int rotate, GBool doLinks,
			 GBool (*abortCheckCbk)(void *data),
			 void *abortCheckCbkData) {
  Page *p;

  if (globalParams->getPrintCommands()) {
    printf("***** page %d *****\n", page);
  }
  p = catalog->getPage(page);
  if (doLinks) {
    if (links) {
      delete links;
    }
    getLinks(p);
    p->display(out, zoom, rotate, links, catalog,
	       abortCheckCbk, abortCheckCbkData);
  } else {
    p->display(out, zoom, rotate, NULL, catalog,
	       abortCheckCbk, abortCheckCbkData);
  }
}

void PDFDoc::displayPages(OutputDev *out, int firstPage, int lastPage,
			  int zoom, int rotate, GBool doLinks,
			  GBool (*abortCheckCbk)(void *data),
			  void *abortCheckCbkData) {
  int page;

  for (page = firstPage; page <= lastPage; ++page) {
    displayPage(out, page, zoom, rotate, doLinks,
		abortCheckCbk, abortCheckCbkData);
  }
}

void PDFDoc::displayPageSlice(OutputDev *out, int page, double zoom,
			      int rotate, int sliceX, int sliceY,
			      int sliceW, int sliceH,
			      GBool (*abortCheckCbk)(void *data),
			      void *abortCheckCbkData) {
  Page *p;

  p = catalog->getPage(page);
  p->displaySlice(out, zoom, rotate, sliceX, sliceY, sliceW, sliceH,
		  NULL, catalog, abortCheckCbk, abortCheckCbkData);
}

GBool PDFDoc::isLinearized() {
  Parser *parser;
  Object obj1, obj2, obj3, obj4, obj5;
  GBool lin;

  lin = gFalse;
  obj1.initNull();
  parser = new Parser(xref,
	     new Lexer(xref,
	       str->makeSubStream(str->getStart(), gFalse, 0, &obj1)));
  parser->getObj(&obj1);
  parser->getObj(&obj2);
  parser->getObj(&obj3);
  parser->getObj(&obj4);
  if (obj1.isInt() && obj2.isInt() && obj3.isCmd("obj") &&
      obj4.isDict()) {
    obj4.dictLookup("Linearized", &obj5);
    if (obj5.isNum() && obj5.getNum() > 0) {
      lin = gTrue;
    }
    obj5.free();
  }
  obj4.free();
  obj3.free();
  obj2.free();
  obj1.free();
  delete parser;
  return lin;
}

GBool PDFDoc::saveAs(GString *name) {
  FILE *f;
  int c;

  if (!(f = fopen(name->getCString(), "wb"))) {
    error(-1, "Couldn't open file '%s'", name->getCString());
    return gFalse;
  }
  str->reset();
  while ((c = str->getChar()) != EOF) {
    fputc(c, f);
  }
  str->close();
  fclose(f);
  return gTrue;
}

void PDFDoc::getLinks(Page *page) {
  Object obj;

  links = new Links(page->getAnnots(&obj), catalog->getBaseURI());
  obj.free();
}
