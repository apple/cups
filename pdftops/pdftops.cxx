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
#include <cups/string.h>
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

int main(int argc, char *argv[]) {
  PDFDoc	*doc;
  GString	*fileName;
  GString	*psFileName;
  PSLevel	level;
  PSOutputDev	*psOut;
  int		num_options;
  cups_option_t	*options;
  const char	*val;
  ppd_file_t	*ppd;
  ppd_size_t	*size;
  FILE		*fp;
  const char	*server_root;
  char		tempfile[1024];
  char		buffer[8192];
  int		bytes;
  int		width, length;
  int		duplex;


  // Make sure status messages are not buffered...
  setbuf(stderr, NULL);

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

  // Default to "Universal" size - min of A4 and Letter...
  width  = 595;
  length = 792;
  level  = psLevel2;
  duplex = 0;

  // Get PPD and initialize options as needed...
  num_options = cupsParseOptions(argv[5], 0, &options);

  if ((ppd = ppdOpenFile(getenv("PPD"))) != NULL)
  {
    fprintf(stderr, "DEBUG: pdftops - opened PPD file \"%s\"...\n", getenv("PPD"));

    ppdMarkDefaults(ppd);
    cupsMarkOptions(ppd, num_options, options);

    if ((size = ppdPageSize(ppd, NULL)) != NULL)
    {
      width  = (int)size->width;
      length = (int)size->length;
    }

    level = ppd->language_level == 1 ? psLevel1 : psLevel2;
  }

  if ((val = cupsGetOption("sides", num_options, options)) != NULL &&
      strncasecmp(val, "two-", 4) == 0)
    duplex = 1;
  else if ((val = cupsGetOption("Duplex", num_options, options)) != NULL &&
           strncasecmp(val, "Duplex", 6) == 0)
    duplex = 1;
  else if ((val = cupsGetOption("JCLDuplex", num_options, options)) != NULL &&
           strncasecmp(val, "Duplex", 6) == 0)
    duplex = 1;
  else if ((val = cupsGetOption("EFDuplex", num_options, options)) != NULL &&
           strncasecmp(val, "Duplex", 6) == 0)
    duplex = 1;
  else if ((val = cupsGetOption("KD03Duplex", num_options, options)) != NULL &&
           strncasecmp(val, "Duplex", 6) == 0)
    duplex = 1;
  else if (ppdIsMarked(ppd, "Duplex", "DuplexNoTumble") ||
           ppdIsMarked(ppd, "Duplex", "DuplexTumble") ||
	   ppdIsMarked(ppd, "JCLDuplex", "DuplexNoTumble") ||
           ppdIsMarked(ppd, "JCLDuplex", "DuplexTumble") ||
	   ppdIsMarked(ppd, "EFDuplex", "DuplexNoTumble") ||
           ppdIsMarked(ppd, "EFDuplex", "DuplexTumble") ||
	   ppdIsMarked(ppd, "KD03Duplex", "DuplexNoTumble") ||
           ppdIsMarked(ppd, "KD03Duplex", "DuplexTumble"))
    duplex = 1;

  cupsFreeOptions(num_options, options);

  if (ppd != NULL)
    ppdClose(ppd);

  fprintf(stderr, "DEBUG: pdftops - level = %d, width = %d, length = %d\n",
          level, width, length);

  // read config file
  if ((server_root = getenv("CUPS_SERVERROOT")) == NULL)
    server_root = CUPS_SERVERROOT;

  snprintf(buffer, sizeof(buffer), "%s/pdftops.conf", server_root);

  globalParams = new GlobalParams(buffer);

  globalParams->setPSPaperWidth(width);
  globalParams->setPSPaperHeight(length);
  globalParams->setPSDuplex(duplex);
  globalParams->setPSLevel(level);
  globalParams->setPSASCIIHex(level == psLevel1);
  globalParams->setPSEmbedType1(1);
  globalParams->setPSEmbedTrueType(1);
  globalParams->setPSEmbedCIDPostScript(1);
  globalParams->setErrQuiet(0);

  // open PDF file
  doc = new PDFDoc(fileName, NULL, NULL, getenv("DEBUG") != NULL);

  // check for print permission
  if (doc->isOk() && doc->okToPrint())
  {
    // CUPS always writes to stdout...
    psFileName = new GString("-");

    // write PostScript file
    psOut = new PSOutputDev(psFileName->getCString(), doc->getXRef(),
                            doc->getCatalog(), 1, doc->getNumPages(),
			    psModePS);
    if (psOut->isOk())
      doc->displayPages(psOut, 1, doc->getNumPages(), 72, 0, gFalse);
    delete psOut;

    // clean up
    delete psFileName;
  }
  else
  {
    error(-1, "Unable to print this document.");
  }

  delete doc;
  delete globalParams;

  // check for memory leaks
  Object::memCheck(stderr);
  gMemReport(stderr);

  // Remove temp file if needed...
  if (tempfile[0])
    unlink(tempfile);

  return 0;
}
