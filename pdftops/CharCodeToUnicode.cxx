//========================================================================
//
// CharCodeToUnicode.cc
//
// Copyright 2001-2003 Glyph & Cog, LLC
//
//========================================================================

#include <config.h>

#ifdef USE_GCC_PRAGMAS
#pragma implementation
#endif

#include <stdio.h>
#include <string.h>
#include "gmem.h"
#include "gfile.h"
#include "GString.h"
#include "Error.h"
#include "GlobalParams.h"
#include "PSTokenizer.h"
#include "CharCodeToUnicode.h"

//------------------------------------------------------------------------

#define maxUnicodeString 8

struct CharCodeToUnicodeString {
  CharCode c;
  Unicode u[maxUnicodeString];
  int len;
};

//------------------------------------------------------------------------

static int getCharFromString(void *data) {
  char *p;
  int c;

  p = *(char **)data;
  if (*p) {
    c = *p++;
    *(char **)data = p;
  } else {
    c = EOF;
  }
  return c;
}

static int getCharFromFile(void *data) {
  return fgetc((FILE *)data);
}

//------------------------------------------------------------------------

CharCodeToUnicode *CharCodeToUnicode::parseCIDToUnicode(GString *collectionA) {
  FILE *f;
  Unicode *mapA;
  CharCode size, mapLenA;
  char buf[64];
  Unicode u;
  CharCodeToUnicode *ctu;

  if (!(f = globalParams->getCIDToUnicodeFile(collectionA))) {
    error(-1, "Couldn't find cidToUnicode file for the '%s' collection",
	  collectionA->getCString());
    return NULL;
  }

  size = 32768;
  mapA = (Unicode *)gmalloc(size * sizeof(Unicode));
  mapLenA = 0;

  while (getLine(buf, sizeof(buf), f)) {
    if (mapLenA == size) {
      size *= 2;
      mapA = (Unicode *)grealloc(mapA, size * sizeof(Unicode));
    }
    if (sscanf(buf, "%x", &u) == 1) {
      mapA[mapLenA] = u;
    } else {
      error(-1, "Bad line (%d) in cidToUnicode file for the '%s' collection",
	    (int)(mapLenA + 1), collectionA->getCString());
      mapA[mapLenA] = 0;
    }
    ++mapLenA;
  }
  fclose(f);

  ctu = new CharCodeToUnicode(collectionA->copy(), mapA, mapLenA, gTrue,
			      NULL, 0);
  gfree(mapA);
  return ctu;
}

CharCodeToUnicode *CharCodeToUnicode::make8BitToUnicode(Unicode *toUnicode) {
  return new CharCodeToUnicode(NULL, toUnicode, 256, gTrue, NULL, 0);
}

CharCodeToUnicode *CharCodeToUnicode::parseCMap(GString *buf, int nBits) {
  CharCodeToUnicode *ctu;
  char *p;

  ctu = new CharCodeToUnicode(NULL);
  p = buf->getCString();
  ctu->parseCMap1(&getCharFromString, &p, nBits);
  return ctu;
}

void CharCodeToUnicode::parseCMap1(int (*getCharFunc)(void *), void *data,
				   int nBits) {
  PSTokenizer *pst;
  char tok1[256], tok2[256], tok3[256];
  int nDigits, n1, n2, n3;
  CharCode oldLen, i;
  CharCode code1, code2;
  Unicode u;
  char uHex[5];
  int j;
  GString *name;
  FILE *f;

  nDigits = nBits / 4;
  pst = new PSTokenizer(getCharFunc, data);
  pst->getToken(tok1, sizeof(tok1), &n1);
  while (pst->getToken(tok2, sizeof(tok2), &n2)) {
    if (!strcmp(tok2, "usecmap")) {
      if (tok1[0] == '/') {
	name = new GString(tok1 + 1);
	if ((f = globalParams->findToUnicodeFile(name))) {
	  parseCMap1(&getCharFromFile, f, nBits);
	  fclose(f);
	} else {
	  error(-1, "Couldn't find ToUnicode CMap file for '%s'",
		name->getCString());
	}
	delete name;
      }
      pst->getToken(tok1, sizeof(tok1), &n1);
    } else if (!strcmp(tok2, "beginbfchar")) {
      while (pst->getToken(tok1, sizeof(tok1), &n1)) {
	if (!strcmp(tok1, "endbfchar")) {
	  break;
	}
	if (!pst->getToken(tok2, sizeof(tok2), &n2) ||
	    !strcmp(tok2, "endbfchar")) {
	  error(-1, "Illegal entry in bfchar block in ToUnicode CMap");
	  break;
	}
	if (!(n1 == 2 + nDigits && tok1[0] == '<' && tok1[n1 - 1] == '>' &&
	      tok2[0] == '<' && tok2[n2 - 1] == '>')) {
	  error(-1, "Illegal entry in bfchar block in ToUnicode CMap");
	  continue;
	}
	tok1[n1 - 1] = tok2[n2 - 1] = '\0';
	if (sscanf(tok1 + 1, "%x", &code1) != 1) {
	  error(-1, "Illegal entry in bfchar block in ToUnicode CMap");
	  continue;
	}
	if (code1 >= mapLen) {
	  oldLen = mapLen;
	  mapLen = (code1 + 256) & ~255;
	  map = (Unicode *)grealloc(map, mapLen * sizeof(Unicode));
	  for (i = oldLen; i < mapLen; ++i) {
	    map[i] = 0;
	  }
	}
	if (n2 == 6) {
	  if (sscanf(tok2 + 1, "%x", &u) != 1) {
	    error(-1, "Illegal entry in bfchar block in ToUnicode CMap");
	    continue;
	  }
	  map[code1] = u;
	} else {
	  map[code1] = 0;
	  if (sMapLen == sMapSize) {
	    sMapSize += 8;
	    sMap = (CharCodeToUnicodeString *)
	        grealloc(sMap, sMapSize * sizeof(CharCodeToUnicodeString));
	  }
	  sMap[sMapLen].c = code1;
	  sMap[sMapLen].len = (n2 - 2) / 4;
	  for (j = 0; j < sMap[sMapLen].len && j < maxUnicodeString; ++j) {
	    strncpy(uHex, tok2 + 1 + j*4, 4);
	    uHex[4] = '\0';
	    if (sscanf(uHex, "%x", &sMap[sMapLen].u[j]) != 1) {
	      error(-1, "Illegal entry in bfchar block in ToUnicode CMap");
	    }
	  }
	  ++sMapLen;
	}
      }
      pst->getToken(tok1, sizeof(tok1), &n1);
    } else if (!strcmp(tok2, "beginbfrange")) {
      while (pst->getToken(tok1, sizeof(tok1), &n1)) {
	if (!strcmp(tok1, "endbfrange")) {
	  break;
	}
	if (!pst->getToken(tok2, sizeof(tok2), &n2) ||
	    !strcmp(tok2, "endbfrange") ||
	    !pst->getToken(tok3, sizeof(tok3), &n3) ||
	    !strcmp(tok3, "endbfrange")) {
	  error(-1, "Illegal entry in bfrange block in ToUnicode CMap");
	  break;
	}
	if (!(n1 == 2 + nDigits && tok1[0] == '<' && tok1[n1 - 1] == '>' &&
	      n2 == 2 + nDigits && tok2[0] == '<' && tok2[n2 - 1] == '>' &&
	      tok3[0] == '<' && tok3[n3 - 1] == '>')) {
	  error(-1, "Illegal entry in bfrange block in ToUnicode CMap");
	  continue;
	}
	tok1[n1 - 1] = tok2[n2 - 1] = tok3[n3 - 1] = '\0';
	if (sscanf(tok1 + 1, "%x", &code1) != 1 ||
	    sscanf(tok2 + 1, "%x", &code2) != 1) {
	  error(-1, "Illegal entry in bfrange block in ToUnicode CMap");
	  continue;
	}
	if (code2 >= mapLen) {
	  oldLen = mapLen;
	  mapLen = (code2 + 256) & ~255;
	  map = (Unicode *)grealloc(map, mapLen * sizeof(Unicode));
	  for (i = oldLen; i < mapLen; ++i) {
	    map[i] = 0;
	  }
	}
	if (n3 <= 6) {
	  if (sscanf(tok3 + 1, "%x", &u) != 1) {
	    error(-1, "Illegal entry in bfrange block in ToUnicode CMap");
	    continue;
	  }
	  for (; code1 <= code2; ++code1) {
	    map[code1] = u++;
	  }
	} else {
	  if (sMapLen + (int)(code2 - code1 + 1) > sMapSize) {
	    sMapSize = (sMapSize + (code2 - code1 + 1) + 7) & ~7;
	    sMap = (CharCodeToUnicodeString *)
	        grealloc(sMap, sMapSize * sizeof(CharCodeToUnicodeString));
	  }
	  for (i = 0; code1 <= code2; ++code1, ++i) {
	    map[code1] = 0;
	    sMap[sMapLen].c = code1;
	    sMap[sMapLen].len = (n3 - 2) / 4;
	    for (j = 0; j < sMap[sMapLen].len && j < maxUnicodeString; ++j) {
	      strncpy(uHex, tok3 + 1 + j*4, 4);
	      uHex[4] = '\0';
	      if (sscanf(uHex, "%x", &sMap[sMapLen].u[j]) != 1) {
		error(-1, "Illegal entry in bfrange block in ToUnicode CMap");
	      }
	    }
	    sMap[sMapLen].u[sMap[sMapLen].len - 1] += i;
	    ++sMapLen;
	  }
	}
      }
      pst->getToken(tok1, sizeof(tok1), &n1);
    } else {
      strcpy(tok1, tok2);
    }
  }
  delete pst;
}

CharCodeToUnicode::CharCodeToUnicode(GString *collectionA) {
  CharCode i;

  collection = collectionA;
  mapLen = 256;
  map = (Unicode *)gmalloc(mapLen * sizeof(Unicode));
  for (i = 0; i < mapLen; ++i) {
    map[i] = 0;
  }
  sMap = NULL;
  sMapLen = sMapSize = 0;
  refCnt = 1;
}

CharCodeToUnicode::CharCodeToUnicode(GString *collectionA, Unicode *mapA,
				     CharCode mapLenA, GBool copyMap,
				     CharCodeToUnicodeString *sMapA,
				     int sMapLenA) {
  collection = collectionA;
  mapLen = mapLenA;
  if (copyMap) {
    map = (Unicode *)gmalloc(mapLen * sizeof(Unicode));
    memcpy(map, mapA, mapLen * sizeof(Unicode));
  } else {
    map = mapA;
  }
  sMap = sMapA;
  sMapLen = sMapSize = sMapLenA;
  refCnt = 1;
}

CharCodeToUnicode::~CharCodeToUnicode() {
  if (collection) {
    delete collection;
  }
  gfree(map);
  if (sMap) {
    gfree(sMap);
  }
}

void CharCodeToUnicode::incRefCnt() {
  ++refCnt;
}

void CharCodeToUnicode::decRefCnt() {
  if (--refCnt == 0) {
    delete this;
  }
}

GBool CharCodeToUnicode::match(GString *collectionA) {
  return collection && !collection->cmp(collectionA);
}

int CharCodeToUnicode::mapToUnicode(CharCode c, Unicode *u, int size) {
  int i, j;

  if (c >= mapLen) {
    return 0;
  }
  if (map[c]) {
    u[0] = map[c];
    return 1;
  }
  for (i = 0; i < sMapLen; ++i) {
    if (sMap[i].c == c) {
      for (j = 0; j < sMap[i].len && j < size; ++j) {
	u[j] = sMap[i].u[j];
      }
      return j;
    }
  }
  return 0;
}

//------------------------------------------------------------------------

CIDToUnicodeCache::CIDToUnicodeCache() {
  int i;

  for (i = 0; i < cidToUnicodeCacheSize; ++i) {
    cache[i] = NULL;
  }
}

CIDToUnicodeCache::~CIDToUnicodeCache() {
  int i;

  for (i = 0; i < cidToUnicodeCacheSize; ++i) {
    if (cache[i]) {
      cache[i]->decRefCnt();
    }
  }
}

CharCodeToUnicode *CIDToUnicodeCache::getCIDToUnicode(GString *collection) {
  CharCodeToUnicode *ctu;
  int i, j;

  if (cache[0] && cache[0]->match(collection)) {
    cache[0]->incRefCnt();
    return cache[0];
  }
  for (i = 1; i < cidToUnicodeCacheSize; ++i) {
    if (cache[i] && cache[i]->match(collection)) {
      ctu = cache[i];
      for (j = i; j >= 1; --j) {
	cache[j] = cache[j - 1];
      }
      cache[0] = ctu;
      ctu->incRefCnt();
      return ctu;
    }
  }
  if ((ctu = CharCodeToUnicode::parseCIDToUnicode(collection))) {
    if (cache[cidToUnicodeCacheSize - 1]) {
      cache[cidToUnicodeCacheSize - 1]->decRefCnt();
    }
    for (j = cidToUnicodeCacheSize - 1; j >= 1; --j) {
      cache[j] = cache[j - 1];
    }
    cache[0] = ctu;
    ctu->incRefCnt();
    return ctu;
  }
  return NULL;
}
