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
#include <config.h>
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
#include <cups/cups.h>

GBool printCommands = gFalse;


int main(int argc, char *argv[]) {
  PDFDoc	*doc;
  GString	*fileName;
  PSOutputDev	*psOut;
  int		num_options;
  cups_option_t	*options;
  ppd_file_t	*ppd;
  ppd_size_t	*size;
  FILE		*fp;
  char		tempfile[1024];
  char		buffer[8192];
  int		bytes;


  // Make sure we have the right number of arguments for CUPS!
  if (argc < 6 || argc > 7)
  {
    fputs("Usage: pdftops job user title copies options [filename]\n", stderr);
    return (1);
  }

  // Copy stdin if needed...
  if (argc == 6)
  {
    if ((fp = fopen(cupsTempFile(tempfile, sizeof(tempfile)), "w")) == NULL)
    {
      perror("ERROR: Unable to copy PDF file");
      return (1);
    }

    fprintf(stderr, "DEBUG: pdftops - copying to temp print file \"%s\"\n",
            tempfile);

    while ((bytes = fread(buffer, 1, sizeof(buffer), stdin)) > 0)
      fwrite(buffer, 1, bytes, fp);
    fclose(fp);

    fileName = new GString(tempfile);
  }
  else
  {
    fileName = new GString(argv[6]);
    tempfile[0] = '\0';
  }

  // Get PPD and initialize options as needed...
  if ((ppd = ppdOpenFile(getenv("PPD"))) != NULL)
  {
    fprintf(stderr, "DEBUG: pdftops - opened PPD file \"%s\"...\n", getenv("PPD"));

    ppdMarkDefaults(ppd);
    num_options = cupsParseOptions(argv[5], 0, &options);
    cupsMarkOptions(ppd, num_options, options);
    cupsFreeOptions(num_options, options);

    if ((size = ppdPageSize(ppd, NULL)) != NULL)
    {
      paperWidth  = size->width;
      paperHeight = size->length;
    }

    psOutLevel1 = ppd->language_level == 1;

    fprintf(stderr, "DEBUG: pstops - psOutLevel1 = %d, paperWidth = %d, paperHeight = %d\n",
            psOutLevel1, paperWidth, paperHeight);

    ppdClose(ppd);
  }

  // init error file
  errorInit();

  // read config file
  initParams(CUPS_SERVERROOT "/xpdf.conf");

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

  // write PostScript file
  psOut = new PSOutputDev("-", doc->getCatalog(), 1, doc->getNumPages(), 1, 0);
  if (psOut->isOk())
    doc->displayPages(psOut, 1, doc->getNumPages(), 72, 0);

  delete psOut;

  // clean up
 err2:
  delete doc;
 err1:
  freeParams();

  // check for memory leaks
  Object::memCheck(stderr);
  gMemReport(stderr);

  // Remove temp file if needed...
  if (tempfile[0])
    unlink(tempfile);

  return 0;
}
