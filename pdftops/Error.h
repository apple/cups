//========================================================================
//
// Error.h
//
// Copyright 1996 Derek B. Noonburg
//
//========================================================================

#ifndef ERROR_H
#define ERROR_H

#ifdef __GNUC__
#pragma interface
#endif

#include <stdio.h>
#include "config.h"

// File to send error (and other) messages to.
extern FILE *errFile;

extern void errorInit();

extern void CDECL error(int pos, const char *msg, ...);

#endif
