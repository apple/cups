//========================================================================
//
// Error.h
//
// Copyright 1996-2002 Glyph & Cog, LLC
//
//========================================================================

#ifndef ERROR_H
#define ERROR_H

#include <config.h>

#ifdef USE_GCC_PRAGMAS
#pragma interface
#endif

#include <stdio.h>
#include "config.h"

extern void CDECL error(int pos, char *msg, ...);

#endif
