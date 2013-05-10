//========================================================================
//
// UnicodeMap.cc
//
// Copyright 2001-2002 Glyph & Cog, LLC
//
//========================================================================

#ifdef __GNUC__
#pragma implementation
#endif

#include <config.h>
#include <stdio.h>
#include <string.h>
#include "gmem.h"
#include "gfile.h"
#include "GString.h"
#include "GList.h"
#include "Error.h"
#include "GlobalParams.h"
#include "UnicodeMap.h"

//------------------------------------------------------------------------

#define maxExtCode 16

struct UnicodeMapExt {
  Unicode u;			// Unicode char
  char code[maxExtCode];
  Guint nBytes;
};

//------------------------------------------------------------------------

UnicodeMap *UnicodeMap::parse(GString *encodingNameA) {
  FILE *f;
  UnicodeMap *map;
  UnicodeMapRange *range;
  UnicodeMapExt *eMap;
  int size, eMapsSize;
  char buf[256];
  int line, nBytes, i, x;
  char *tok1, *tok2, *tok3;

  if (!(f = globalParams->getUnicodeMapFile(encodingNameA))) {
    error(-1, "Couldn't find unicodeMap file for the '%s' encoding",
	  encodingNameA->getCString());
    return NULL;
  }

  map = new UnicodeMap(encodingNameA->copy());

  size = 8;
  map->ranges = (UnicodeMapRange *)gmalloc(size * sizeof(UnicodeMapRange));
  eMapsSize = 0;

  line = 1;
  while (getLine(buf, sizeof(buf), f)) {
    if ((tok1 = strtok(buf, " \t\r\n")) &&
	(tok2 = strtok(NULL, " \t\r\n"))) {
      if (!(tok3 = strtok(NULL, " \t\r\n"))) {
	tok3 = tok2;
	tok2 = tok1;
      }
      nBytes = strlen(tok3) / 2;
      if (nBytes <= 4) {
	if (map->len == size) {
	  size *= 2;
	  map->ranges = (UnicodeMapRange *)
	    grealloc(map->ranges, size * sizeof(UnicodeMapRange));
	}
	range = &map->ranges[map->len];
	sscanf(tok1, "%x", &range->start);
	sscanf(tok2, "%x", &range->end);
	sscanf(tok3, "%x", &range->code);
	range->nBytes = nBytes;
	++map->len;
      } else if (tok2 == tok1) {
	if (map->eMapsLen == eMapsSize) {
	  eMapsSize += 16;
	  map->eMaps = (UnicodeMapExt *)
	    grealloc(map->eMaps, eMapsSize * sizeof(UnicodeMapExt));
	}
	eMap = &map->eMaps[map->eMapsLen];
	sscanf(tok1, "%x", &eMap->u);
	for (i = 0; i < nBytes; ++i) {
	  sscanf(tok3 + i*2, "%2x", &x);
	  eMap->code[i] = (char)x;
	}
	eMap->nBytes = nBytes;
	++map->eMapsLen;
      } else {
	error(-1, "Bad line (%d) in unicodeMap file for the '%s' encoding",
	      line, encodingNameA->getCString());
      }
    } else {
      error(-1, "Bad line (%d) in unicodeMap file for the '%s' encoding",
	    line, encodingNameA->getCString());
    }
    ++line;
  }

  return map;
}

UnicodeMap::UnicodeMap(GString *encodingNameA) {
  encodingName = encodingNameA;
  kind = unicodeMapUser;
  ranges = NULL;
  len = 0;
  eMaps = NULL;
  eMapsLen = 0;
  refCnt = 1;
}

UnicodeMap::UnicodeMap(char *encodingNameA,
		       UnicodeMapRange *rangesA, int lenA) {
  encodingName = new GString(encodingNameA);
  kind = unicodeMapResident;
  ranges = rangesA;
  len = lenA;
  eMaps = NULL;
  eMapsLen = 0;
  refCnt = 1;
}

UnicodeMap::UnicodeMap(char *encodingNameA, UnicodeMapFunc funcA) {
  encodingName = new GString(encodingNameA);
  kind = unicodeMapFunc;
  func = funcA;
  eMaps = NULL;
  eMapsLen = 0;
  refCnt = 1;
}

UnicodeMap::~UnicodeMap() {
  delete encodingName;
  if (kind == unicodeMapUser && ranges) {
    gfree(ranges);
  }
  if (eMaps) {
    gfree(eMaps);
  }
}

void UnicodeMap::incRefCnt() {
  ++refCnt;
}

void UnicodeMap::decRefCnt() {
  if (--refCnt == 0) {
    delete this;
  }
}

GBool UnicodeMap::match(GString *encodingNameA) {
  return !encodingName->cmp(encodingNameA);
}

int UnicodeMap::mapUnicode(Unicode u, char *buf, int bufSize) {
  int a, b, m, n, i, j;
  Guint code;

  if (kind == unicodeMapFunc) {
    return (*func)(u, buf, bufSize);
  }

  a = 0;
  b = len;
  if (u < ranges[a].start) {
    return 0;
  }
  // invariant: ranges[a].start <= u < ranges[b].start
  while (b - a > 1) {
    m = (a + b) / 2;
    if (u >= ranges[m].start) {
      a = m;
    } else if (u < ranges[m].start) {
      b = m;
    }
  }
  if (u <= ranges[a].end) {
    n = ranges[a].nBytes;
    if (n > bufSize) {
      return 0;
    }
    code = ranges[a].code + (u - ranges[a].start);
    for (i = n - 1; i >= 0; --i) {
      buf[i] = (char)(code & 0xff);
      code >>= 8;
    }
    return n;
  }

  for (i = 0; i < eMapsLen; ++i) {
    if (eMaps[i].u == u) {
      n = eMaps[i].nBytes;
      for (j = 0; j < n; ++j) {
	buf[j] = eMaps[i].code[j];
      }
      return n;
    }
  }

  return 0;
}

//------------------------------------------------------------------------

UnicodeMapCache::UnicodeMapCache() {
  int i;

  for (i = 0; i < unicodeMapCacheSize; ++i) {
    cache[i] = NULL;
  }
}

UnicodeMapCache::~UnicodeMapCache() {
  int i;

  for (i = 0; i < unicodeMapCacheSize; ++i) {
    if (cache[i]) {
      cache[i]->decRefCnt();
    }
  }
}

UnicodeMap *UnicodeMapCache::getUnicodeMap(GString *encodingName) {
  UnicodeMap *map;
  int i, j;

  if (cache[0] && cache[0]->match(encodingName)) {
    cache[0]->incRefCnt();
    return cache[0];
  }
  for (i = 1; i < unicodeMapCacheSize; ++i) {
    if (cache[i] && cache[i]->match(encodingName)) {
      map = cache[i];
      for (j = i; j >= 1; --j) {
	cache[j] = cache[j - 1];
      }
      cache[0] = map;
      map->incRefCnt();
      return map;
    }
  }
  if ((map = UnicodeMap::parse(encodingName))) {
    if (cache[unicodeMapCacheSize - 1]) {
      cache[unicodeMapCacheSize - 1]->decRefCnt();
    }
    for (j = unicodeMapCacheSize - 1; j >= 1; --j) {
      cache[j] = cache[j - 1];
    }
    cache[0] = map;
    map->incRefCnt();
    return map;
  }
  return NULL;
}
