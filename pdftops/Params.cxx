//========================================================================
//
// Params.cc
//
// Copyright 1996 Derek B. Noonburg
//
//========================================================================

#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "gtypes.h"
#include "gmem.h"
#include "GString.h"
#include "gfile.h"
#include "Params.h"

const char **fontPath = NULL;
static int fontPathLen, fontPathSize;

DevFontMapEntry *devFontMap = NULL;
static int devFontMapLen, devFontMapSize;

void initParams(const char *configFile) {
  GString *fileName;
  FILE *f;
  char buf[256];
  char *p, *q;

  // initialize font path and font map
  fontPath = (const char **)gmalloc((fontPathSize = 8) * sizeof(const char *));
  fontPath[fontPathLen = 0] = NULL;
  devFontMap = (DevFontMapEntry *)gmalloc((devFontMapSize = 8) *
					  sizeof(DevFontMapEntry));
  devFontMap[devFontMapLen = 0].pdfFont = NULL;

  // read config file
  fileName = appendToPath(getHomeDir(), configFile);
  if ((f = fopen(fileName->getCString(), "r"))) {
    while (fgets(buf, sizeof(buf)-1, f)) {
      buf[sizeof(buf)-1] = '\0';
      p = strtok(buf, " \t\n\r");
      if (p && !strcmp(p, "fontpath")) {
	if (fontPathLen+1 >= fontPathSize)
	  fontPath = (const char **)
	      grealloc(fontPath, (fontPathSize += 8) * sizeof(const char *));
	p = strtok(NULL, " \t\n\r");
	fontPath[fontPathLen++] = copyString(p);
      } else if (p && !strcmp(p, "fontmap")) {
	if (devFontMapLen+1 >= devFontMapSize)
	  devFontMap = (DevFontMapEntry *)
	      grealloc(devFontMap,
		       (devFontMapSize += 8) * sizeof(DevFontMapEntry));
	p = strtok(NULL, " \t\n\r");
	devFontMap[devFontMapLen].pdfFont = copyString(p);
	p = strtok(NULL, "\t\n\r");
	while (*p == ' ')
	  ++p;
	for (q = p + strlen(p) - 1; q >= p && *q == ' '; --q) ;
	q[1] = '\0';
	devFontMap[devFontMapLen++].devFont = copyString(p);
      }
    }
    fclose(f);
    fontPath[fontPathLen] = NULL;
    devFontMap[devFontMapLen].pdfFont = NULL;
  }
  delete fileName;
}

void freeParams() {
  int i;

  if (fontPath) {
    for (i = 0; i < fontPathLen; ++i)
      gfree((void *)fontPath[i]);
    gfree((void *)fontPath);
  }
  if (devFontMap) {
    for (i = 0; i < devFontMapLen; ++i) {
      gfree((void *)devFontMap[i].pdfFont);
      gfree((void *)devFontMap[i].devFont);
    }
    gfree((void *)devFontMap);
  }
}
