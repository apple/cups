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

#ifdef HAVE_LIBCUPS
#  include <cups/cups.h>
#endif /* HAVE_LIBCUPS */

static int firstPage = 1;
static int lastPage = 0;
static GBool noEmbedFonts = gFalse;
static GBool doForm = gFalse;
static char userPassword[33] = "";
static GBool printVersion = gFalse;
static GBool printHelp = gFalse;

static ArgDesc argDesc[] = {
  {"-f",      argInt,      &firstPage,      0,
   "first page to print"},
  {"-l",      argInt,      &lastPage,       0,
   "last page to print"},
  {"-paperw", argInt,      &paperWidth,     0,
   "paper width, in points"},
  {"-paperh", argInt,      &paperHeight,    0,
   "paper height, in points"},
  {"-level1", argFlag,     &psOutLevel1,    0,
   "generate Level 1 PostScript"},
  {"-level1sep", argFlag,  &psOutLevel1Sep, 0,
   "generate Level 1 separable PostScript"},
  {"-eps",    argFlag,     &psOutEPS,       0,
   "generate Encapsulated PostScript (EPS)"},
#if OPI_SUPPORT
  {"-opi",    argFlag,     &psOutOPI,       0,
   "generate OPI comments"},
#endif
  {"-noemb",  argFlag,     &noEmbedFonts,   0,
   "don't embed Type 1 fonts"},
  {"-form",   argFlag,     &doForm,         0,
   "generate a PostScript form"},
  {"-upw",    argString,   userPassword,    sizeof(userPassword),
   "user password (for encrypted files)"},
  {"-q",      argFlag,     &errQuiet,       0,
   "don't print any messages or errors"},
  {"-v",      argFlag,     &printVersion,   0,
   "print copyright and version info"},
  {"-h",      argFlag,     &printHelp,      0,
   "print usage information"},
  {"-help",   argFlag,     &printHelp,      0,
   "print usage information"},
  {NULL}
};

int main(int argc, char *argv[]) {
  PDFDoc *doc;
  GString *fileName;
  GString *psFileName;
  GString *userPW;
  PSOutputDev *psOut;
  GBool ok;
  char *p;
#ifdef HAVE_LIBCUPS
  int		num_options;
  cups_option_t	*options;
  ppd_file_t	*ppd;
  ppd_size_t	*size;
  FILE		*fp;
  const char	*server_root;
  char		tempfile[1024];
  char		buffer[8192];
  int		bytes;


  // See if we are being run as a filter...
  if (getenv("PPD") && getenv("SOFTWARE")) {
    // Yes, make sure status messages are not buffered...
    setbuf(stderr, NULL);

    // Send all error messages...
    errQuiet = 0;

    // Make sure we have the right number of arguments for CUPS!
    if (argc < 6 || argc > 7) {
      fputs("Usage: pdftops job user title copies options [filename]\n", stderr);
      return (1);
    }

    // Copy stdin if needed...
    if (argc == 6) {
      if ((fp = fopen(cupsTempFile(tempfile, sizeof(tempfile)), "w")) == NULL) {
	perror("ERROR: Unable to copy PDF file");
	return (1);
      }

      fprintf(stderr, "DEBUG: pdftops - copying to temp print file \"%s\"\n",
              tempfile);

      while ((bytes = fread(buffer, 1, sizeof(buffer), stdin)) > 0)
	fwrite(buffer, 1, bytes, fp);
      fclose(fp);

      fileName = new GString(tempfile);
    } else {
      fileName = new GString(argv[6]);
      tempfile[0] = '\0';
    }
  } else {
    tempfile[0] = '\0';
#endif // HAVE_LIBCUPS
  // parse args
  ok = parseArgs(argDesc, &argc, argv);
  if (!ok || argc < 2 || argc > 3 || printVersion || printHelp) {
    fprintf(stderr, "pdftops version %s\n", xpdfVersion);
    fprintf(stderr, "%s\n", xpdfCopyright);
    if (!printVersion) {
      printUsage("pdftops", "<PDF-file> [<PS-file>]", argDesc);
    }
    exit(1);
  }
  if (psOutLevel1 && psOutLevel1Sep) {
    fprintf(stderr, "Error: use -level1 or -level1sep, not both.\n");
    exit(1);
  }
  if (doForm && (psOutLevel1 || psOutLevel1Sep)) {
    fprintf(stderr, "Error: forms are only available with Level 2 output.\n");
    exit(1);
  }
  fileName = new GString(argv[1]);
#ifdef HAVE_LIBCUPS
  }

  // Get PPD and initialize options as needed...
  if ((ppd = ppdOpenFile(getenv("PPD"))) != NULL) {
    fprintf(stderr, "DEBUG: pdftops - opened PPD file \"%s\"...\n", getenv("PPD"));

    ppdMarkDefaults(ppd);
    num_options = cupsParseOptions(argv[5], 0, &options);
    cupsMarkOptions(ppd, num_options, options);
    cupsFreeOptions(num_options, options);

    if ((size = ppdPageSize(ppd, NULL)) != NULL) {
      paperWidth  = (int)size->width;
      paperHeight = (int)size->length;
    }

    psOutLevel1 = ppd->language_level == 1;

    fprintf(stderr, "DEBUG: pdftops - psOutLevel1 = %d, paperWidth = %d, paperHeight = %d\n",
            psOutLevel1, paperWidth, paperHeight);

    ppdClose(ppd);
  }
#endif // HAVE_LIBCUPS

  // init error file
  errorInit();

  // read config file
#ifdef HAVE_LIBCUPS
  if ((server_root = getenv("CUPS_SERVERROOT")) != NULL) {
    sprintf(tempfile, "%s/pdftops.conf", server_root);
    initParams(tempfile);
  } else
#endif /* HAVE_LIBCUPS */
  initParams(xpdfConfigFile);

  // open PDF file
  xref = NULL;
  if (userPassword[0]) {
    userPW = new GString(userPassword);
  } else {
    userPW = NULL;
  }
  doc = new PDFDoc(fileName, userPW);
  if (userPW) {
    delete userPW;
  }
  if (!doc->isOk()) {
    goto err1;
  }

  // check for print permission
  if (!doc->okToPrint()) {
    error(-1, "Printing this document is not allowed.");
    goto err1;
  }

#ifdef HAVE_LIBCUPS
  if (getenv("PPD") && getenv("SOFTWARE")) {
    // CUPS always needs every page and writes to stdout...
    psFileName = new GString("-");
    firstPage  = 1;
    lastPage   = doc->getNumPages();
  } else {
#endif // HAVE_LIBCUPS

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
    psFileName->append(psOutEPS ? ".eps" : ".ps");
  }

  // get page range
  if (firstPage < 1)
    firstPage = 1;
  if (lastPage < 1 || lastPage > doc->getNumPages())
    lastPage = doc->getNumPages();
  if (doForm)
    lastPage = firstPage;

  // check for multi-page EPS
  if (psOutEPS && firstPage != lastPage) {
    error(-1, "EPS files can only contain one page.");
    goto err2;
  }
#ifdef HAVE_LIBCUPS
  }
#endif // HAVE_LIBCUPS

  // write PostScript file
  psOut = new PSOutputDev(psFileName->getCString(), doc->getCatalog(),
			  firstPage, lastPage, !noEmbedFonts, doForm);
  if (psOut->isOk())
    doc->displayPages(psOut, firstPage, lastPage, 72, 0, gFalse);
  delete psOut;

  // clean up
 err2:
  delete psFileName;
 err1:
  delete doc;
  freeParams();

  // check for memory leaks
  Object::memCheck(stderr);
  gMemReport(stderr);

#ifdef HAVE_LIBCUPS
  // Remove temp file if needed...
  if (tempfile[0])
    unlink(tempfile);
#endif /* HAVE_LIBCUPS */

  return 0;
}
