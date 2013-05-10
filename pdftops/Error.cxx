//========================================================================
//
// Error.cc
//
// Copyright 1996 Derek B. Noonburg
//
//========================================================================

#ifdef __GNUC__
#pragma implementation
#endif

#include <stdio.h>
#include <stddef.h>
#include <stdarg.h>
#include "gtypes.h"
#include "Params.h"
#include "Error.h"

// Send error messages to /dev/tty instead of stderr.
GBool errorsToTTY = gFalse;

FILE *errFile;
GBool errQuiet;

void errorInit() {
  errFile = stderr;
}

void CDECL error(int pos, char *msg, ...) {
  va_list args;

  if (errQuiet) {
    return;
  }
  if (printCommands) {
    fflush(stdout);
  }
  if (pos >= 0) {
    fprintf(errFile, "ERROR: (%d): ", pos);
  } else {
    fprintf(errFile, "ERROR: ");
  }
  va_start(args, msg);
  vfprintf(errFile, msg, args);
  va_end(args);
  fprintf(errFile, "\n");
  fflush(errFile);
}
