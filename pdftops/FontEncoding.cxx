//========================================================================
//
// FontEncoding.cc
//
// Copyright 1999 Derek B. Noonburg
//
//========================================================================

#ifdef __GNUC__
#pragma implementation
#endif

#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "gmem.h"
#include "FontEncoding.h"

//------------------------------------------------------------------------
// FontEncoding
//------------------------------------------------------------------------

inline int FontEncoding::hash(char *name) {
  int h;

  h = name[0];
  if (name[1])
    h = h * 61 + name[1];
  return h % fontEncHashSize;
}

FontEncoding::FontEncoding() {
  int i;

  encoding = (char **)gmalloc(256 * sizeof(char *));
  size = 256;
  freeEnc = gTrue;
  for (i = 0; i < 256; ++i)
    encoding[i] = NULL;
  for (i = 0; i < fontEncHashSize; ++i)
    hashTab[i] = -1;
}

FontEncoding::FontEncoding(char **encoding, int size) {
  int i;

  this->encoding = encoding;
  this->size = size;
  freeEnc = gFalse;
  for (i = 0; i < fontEncHashSize; ++i)
    hashTab[i] = -1;
  for (i = 0; i < size; ++i) {
    if (encoding[i])
      addChar1(i, encoding[i]);
  }
}

FontEncoding::FontEncoding(FontEncoding *fontEnc) {
  int i;

  encoding = (char **)gmalloc(fontEnc->size * sizeof(char *));
  size = fontEnc->size;
  freeEnc = gTrue;
  for (i = 0; i < size; ++i) {
    encoding[i] =
      fontEnc->encoding[i] ? copyString(fontEnc->encoding[i]) : NULL;
  }
  memcpy(hashTab, fontEnc->hashTab, fontEncHashSize * sizeof(short));
}

void FontEncoding::addChar(int code, char *name) {
  int h, i;

  // replace character associated with code
  if (encoding[code]) {
    h = hash(encoding[code]);
    for (i = 0; i < fontEncHashSize; ++i) {
      if (hashTab[h] == code) {
	hashTab[h] = -2;
	break;
      }
      if (++h == fontEncHashSize)
	h = 0;
    }
    gfree(encoding[code]);
  }

  // associate name with code
  encoding[code] = name;

  // insert name in hash table
  addChar1(code, name);
}

void FontEncoding::addChar1(int code, char *name) {
  int h, i, code2;

  // insert name in hash table
  h = hash(name); 
  for (i = 0; i < fontEncHashSize; ++i) {
    code2 = hashTab[h];
    if (code2 < 0) {
      hashTab[h] = code;
      break;
    } else if (encoding[code2] && !strcmp(encoding[code2], name)) {
      // keep the highest code for each char -- this is needed because
      // X won't display chars with codes < 32
      if (code > code2)
	hashTab[h] = code;
      break;
    }
    if (++h == fontEncHashSize)
      h = 0;
  }
}

FontEncoding::~FontEncoding() {
  int i;

  if (freeEnc) {
    for (i = 0; i < size; ++i) {
      if (encoding[i])
	gfree(encoding[i]);
    }
    gfree(encoding);
  }
}

int FontEncoding::getCharCode(char *name) {
  int h, i, code;

  h = hash(name);
  for (i = 0; i < fontEncHashSize; ++i) {
    code = hashTab[h];
    if (code == -1 ||
	(code >= 0 && encoding[code] && !strcmp(encoding[code], name)))
      return code;
    if (++h >= fontEncHashSize)
      h = 0;
  }
  return -1;
}
