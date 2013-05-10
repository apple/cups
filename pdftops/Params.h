//========================================================================
//
// Params.h
//
// Copyright 1996 Derek B. Noonburg
//
//========================================================================

#ifndef PARAMS_H
#define PARAMS_H

#include "gtypes.h"

// If this is set, error messages will be silently discarded.
extern GBool errQuiet;

// Font search path.
extern char **fontPath;

// Mapping from PDF font name to device font name.
struct DevFontMapEntry {
  char *pdfFont;
  char *devFont;
};
extern DevFontMapEntry *devFontMap;

//------------------------------------------------------------------------

// Initialize font path and font map, and read configuration file.  If
// <userConfigFile> exists, read it; else if <sysConfigFile> exists,
// read it.  <userConfigFile> is relative to the user's home
// directory; <sysConfigFile> should be an absolute path.
extern void initParams(char *userConfigFile, char *sysConfigFile);

// Free memory used for font path and font map.
extern void freeParams();

#endif
