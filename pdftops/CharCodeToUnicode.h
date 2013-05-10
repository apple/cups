//========================================================================
//
// CharCodeToUnicode.h
//
// Mapping from character codes to Unicode.
//
// Copyright 2001-2002 Glyph & Cog, LLC
//
//========================================================================

#ifndef CHARCODETOUNICODE_H
#define CHARCODETOUNICODE_H

#include <config.h>

#ifdef USE_GCC_PRAGMAS
#pragma interface
#endif

#include "CharTypes.h"

struct CharCodeToUnicodeString;

//------------------------------------------------------------------------

class CharCodeToUnicode {
public:

  // Create the CID-to-Unicode mapping specified by <collection>.
  // This reads a .cidToUnicode file from disk.  Sets the initial
  // reference count to 1.  Returns NULL on failure.
  static CharCodeToUnicode *parseCIDToUnicode(GString *collectionA);

  // Create the CharCode-to-Unicode mapping for an 8-bit font.
  // <toUnicode> is an array of 256 Unicode indexes.  Sets the initial
  // reference count to 1.
  static CharCodeToUnicode *make8BitToUnicode(Unicode *toUnicode);

  // Parse a ToUnicode CMap for an 8- or 16-bit font.
  static CharCodeToUnicode *parseCMap(GString *buf, int nBits);

  ~CharCodeToUnicode();

  void incRefCnt();
  void decRefCnt();

  // Return true if this mapping matches the specified <collectionA>.
  GBool match(GString *collectionA);

  // Map a CharCode to Unicode.
  int mapToUnicode(CharCode c, Unicode *u, int size);

private:

  void parseCMap1(int (*getCharFunc)(void *), void *data, int nBits);
  CharCodeToUnicode(GString *collectionA);
  CharCodeToUnicode(GString *collectionA, Unicode *mapA,
		    CharCode mapLenA, GBool copyMap,
		    CharCodeToUnicodeString *sMapA, int sMapLenA);

  GString *collection;
  Unicode *map;
  CharCode mapLen;
  CharCodeToUnicodeString *sMap;
  int sMapLen, sMapSize;
  int refCnt;
};

//------------------------------------------------------------------------

#define cidToUnicodeCacheSize 4

class CIDToUnicodeCache {
public:

  CIDToUnicodeCache();
  ~CIDToUnicodeCache();

  // Get the CharCodeToUnicode object for <collection>.  Increments
  // its reference count; there will be one reference for the cache
  // plus one for the caller of this function.  Returns NULL on
  // failure.
  CharCodeToUnicode *getCIDToUnicode(GString *collection);

private:

  CharCodeToUnicode *cache[cidToUnicodeCacheSize];
};

#endif
