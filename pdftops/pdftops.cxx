//========================================================================
//
// pdftops.cc
//
// Copyright 1996 Derek B. Noonburg
//
//========================================================================

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include "parseargs.h"
#include "GString.h"
#include "gmem.h"
#include "Object.h"
#include "Stream.h"
#include "Array.h"
#include "Dict.h"
#include "XRef.h"
#include "Catalog.h"
#include "Page.h"
#include "PDFDoc.h"
#include "PSOutputDev.h"
#include "Params.h"
#include "Error.h"
#include "config.h"

static int firstPage = 1;
static int lastPage = 0;
static GBool noEmbedFonts = gFalse;
static GBool doForm = gFalse;
GBool printCommands = gFalse;
static GBool printHelp = gFalse;

static ArgDesc argDesc[] = {
  {"-f",      argInt,      &firstPage,     0,
   "first page to print"},
  {"-l",      argInt,      &lastPage,      0,
   "last page to print"},
  {"-paperw", argInt,      &paperWidth,    0,
   "paper width, in points"},
  {"-paperh", argInt,      &paperHeight,   0,
   "paper height, in points"},
  {"-level1", argFlag,     &psOutLevel1,   0,
   "generate Level 1 PostScript"},
  {"-noemb",  argFlag,     &noEmbedFonts,  0,
   "don't embed Type 1 fonts"},
  {"-form",   argFlag,     &doForm,        0,
   "generate a PostScript form"},
  {"-q",      argFlag,     &errQuiet,      0,
   "don't print any messages or errors"},
  {"-h",      argFlag,     &printHelp,     0,
   "print usage information"},
  {"-help",   argFlag,     &printHelp,     0,
   "print usage information"},
  {NULL}
};

int main(int argc, char *argv[]) {
  PDFDoc *doc;
  GString *fileName;
  GString *psFileName;
  PSOutputDev *psOut;
  GBool ok;
  char *p;

  // parse args
  ok = parseArgs(argDesc, &argc, argv);
  if (!ok || argc < 2 || argc > 3 || printHelp) {
    fprintf(stderr, "pdftops version %s\n", xpdfVersion);
    fprintf(stderr, "%s\n", xpdfCopyright);
    printUsage("pdftops", "<PDF-file> [<PS-file>]", argDesc);
    exit(1);
  }
  if (doForm && psOutLevel1) {
    fprintf(stderr, "Error: forms are only available with Level 2 output.\n");
    exit(1);
  }
  fileName = new GString(argv[1]);

  // init error file
  errorInit();

  // read config file
  initParams(xpdfConfigFile);

  // open PDF file
  xref = NULL;
  doc = new PDFDoc(fileName);
  if (!doc->isOk()) {
    goto err1;
  }

  // check for print permission
  if (!doc->okToPrint()) {
    error(-1, "Printing this document is not allowed.");
    goto err2;
  }

  // construct PostScript file name
  if (argc == 3) {
    psFileName = new GString(argv[2]);
  } else {
    p = fileName->getCString() + fileName->getLength() - 4;
    if (!strcmp(p, ".pdf") || !strcmp(p, ".PDF"))
      psFileName = new GString(fileName->getCString(),
			       fileName->getLength() - 4);
    else
      psFileName = fileName->copy();
    psFileName->append(".ps");
  }

  // get page range
  if (firstPage < 1)
    firstPage = 1;
  if (lastPage < 1 || lastPage > doc->getNumPages())
    lastPage = doc->getNumPages();
  if (doForm)
    lastPage = firstPage;

  // write PostScript file
  psOut = new PSOutputDev(psFileName->getCString(), doc->getCatalog(),
			  firstPage, lastPage, !noEmbedFonts, doForm);
  if (psOut->isOk())
    doc->displayPages(psOut, firstPage, lastPage, 72, 0);
  delete psOut;

  // clean up
  delete psFileName;
 err2:
  delete doc;
 err1:
  freeParams();

  // check for memory leaks
  Object::memCheck(stderr);
  gMemReport(stderr);

  return 0;
}
