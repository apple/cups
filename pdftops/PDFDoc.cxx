//========================================================================
//
// PDFDoc.cc
//
// Copyright 1996 Derek B. Noonburg
//
//========================================================================

#ifdef __GNUC__
#pragma implementation
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include "GString.h"
#include "config.h"
#include "Page.h"
#include "Catalog.h"
#include "XRef.h"
#include "Link.h"
#include "OutputDev.h"
#include "Params.h"
#include "Error.h"
#include "PDFDoc.h"

//------------------------------------------------------------------------
// PDFDoc
//------------------------------------------------------------------------

PDFDoc::PDFDoc(GString *fileName1) {
  FileStream *str;
  Object catObj;
  Object obj;
  GString *fileName2;

  // setup
  ok = gFalse;
  catalog = NULL;
  xref = NULL;
  file = NULL;
  links = NULL;

  // try to open file
  fileName = fileName1;
  fileName2 = NULL;
#ifdef VMS
  if (!(file = fopen(fileName->getCString(), "rb", "ctx=stm"))) {
    error(-1, "Couldn't open file '%s'", fileName->getCString());
    return;
  }
#else
  if (!(file = fopen(fileName->getCString(), "rb"))) {
    fileName2 = fileName->copy();
    fileName2->lowerCase();
    if (!(file = fopen(fileName2->getCString(), "rb"))) {
      fileName2->upperCase();
      if (!(file = fopen(fileName2->getCString(), "rb"))) {
	error(-1, "Couldn't open file '%s'", fileName->getCString());
	delete fileName2;
	return;
      }
    }
    delete fileName2;
  }
#endif

  // create stream
  obj.initNull();
  str = new FileStream(file, 0, -1, &obj);

  // check header
  str->checkHeader();

  // read xref table
  xref = new XRef(str);
  delete str;
  if (!xref->isOk()) {
    error(-1, "Couldn't read xref table");
    return;
  }

  // read catalog
  catalog = new Catalog(xref->getCatalog(&catObj));
  catObj.free();
  if (!catalog->isOk()) {
    error(-1, "Couldn't read page catalog");
    return;
  }

  // done
  ok = gTrue;
  return;
}

PDFDoc::~PDFDoc() {
  if (catalog)
    delete catalog;
  if (xref)
    delete xref;
  if (file)
    fclose(file);
  if (fileName)
    delete fileName;
  if (links)
    delete links;
}

void PDFDoc::displayPage(OutputDev *out, int page, int zoom, int rotate,
			 GBool doLinks) {
  Link *link;
  double x1, y1, x2, y2;
  double w;
  int i;

  if (printCommands)
    printf("***** page %d *****\n", page);
  catalog->getPage(page)->display(out, zoom, rotate);
  if (doLinks) {
    if (links)
      delete links;
    getLinks(page);
    for (i = 0; i < links->getNumLinks(); ++i) {
      link = links->getLink(i);
      link->getBorder(&x1, &y1, &x2, &y2, &w);
      if (w > 0)
	out->drawLinkBorder(x1, y1, x2, y2, w);
    }
    out->dump();
  }
}

void PDFDoc::displayPages(OutputDev *out, int firstPage, int lastPage,
			  int zoom, int rotate) {
  Page *p;
  int page;

  for (page = firstPage; page <= lastPage; ++page) {
    if (printCommands)
      printf("***** page %d *****\n", page);
    p = catalog->getPage(page);
    p->display(out, zoom, rotate);
  }
}

GBool PDFDoc::saveAs(GString *name) {
  FILE *f;
  char buf[4096];
  int n;

  if (!(f = fopen(name->getCString(), "wb"))) {
    error(-1, "Couldn't open file '%s'", name->getCString());
    return gFalse;
  }
  rewind(file);
  while ((n = fread(buf, 1, sizeof(buf), file)) > 0)
    fwrite(buf, 1, n, f);
  fclose(f);
  return gTrue;
}

void PDFDoc::getLinks(int page) {
  Object obj;

  links = new Links(catalog->getPage(page)->getAnnots(&obj),
		    catalog->getBaseURI());
  obj.free();
}
