//========================================================================
//
// PSTokenizer.h
//
// Copyright 2002 Glyph & Cog, LLC
//
//========================================================================

#ifndef PSTOKENIZER_H
#define PSTOKENIZER_H

#include <config.h>

#ifdef USE_GCC_PRAGMAS
#pragma interface
#endif

#include "gtypes.h"

//------------------------------------------------------------------------

class PSTokenizer {
public:

  PSTokenizer(int (*getCharFuncA)(void *), void *dataA);
  ~PSTokenizer();

  // Get the next PostScript token.  Returns false at end-of-stream.
  GBool getToken(char *buf, int size, int *length);

private:

  int lookChar();
  int getChar();

  int (*getCharFunc)(void *);
  void *data;
  int charBuf;
};

#endif
