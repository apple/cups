//========================================================================
//
// Error.h
//
// Copyright 1996-2002 Glyph & Cog, LLC
//
//========================================================================

#ifndef ERROR_H
#define ERROR_H

#ifdef __GNUC__
#pragma interface
#endif

#include <stdio.h>
#include "config.h"

extern void CDECL error(int pos, const char *msg, ...);

#endif
