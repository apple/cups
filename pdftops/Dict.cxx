//========================================================================
//
// Dict.cc
//
// Copyright 1996-2003 Glyph & Cog, LLC
//
//========================================================================

#include <config.h>

#ifdef USE_GCC_PRAGMAS
#pragma implementation
#endif

#include <stddef.h>
#include <string.h>
#include "gmem.h"
#include "Object.h"
#include "XRef.h"
#include "Dict.h"

//------------------------------------------------------------------------
// Dict
//------------------------------------------------------------------------

Dict::Dict(XRef *xrefA) {
  xref = xrefA;
  entries = NULL;
  size = length = 0;
  ref = 1;
}

Dict::~Dict() {
  int i;

  for (i = 0; i < length; ++i) {
    gfree((void *)entries[i].key);
    entries[i].val.free();
  }
  gfree(entries);
}

void Dict::add(const char *key, Object *val) {
  if (length + 1 > size) {
    size += 8;
    entries = (DictEntry *)grealloc(entries, size * sizeof(DictEntry));
  }
  entries[length].key = key;
  entries[length].val = *val;
  ++length;
}

inline DictEntry *Dict::find(const char *key) {
  int i;

  for (i = 0; i < length; ++i) {
    if (!strcmp(key, entries[i].key))
      return &entries[i];
  }
  return NULL;
}

GBool Dict::is(const char *type) {
  DictEntry *e;

  return (e = find("Type")) && e->val.isName(type);
}

Object *Dict::lookup(const char *key, Object *obj) {
  DictEntry *e;

  return (e = find(key)) ? e->val.fetch(xref, obj) : obj->initNull();
}

Object *Dict::lookupNF(const char *key, Object *obj) {
  DictEntry *e;

  return (e = find(key)) ? e->val.copy(obj) : obj->initNull();
}

const char *Dict::getKey(int i) {
  return entries[i].key;
}

Object *Dict::getVal(int i, Object *obj) {
  return entries[i].val.fetch(xref, obj);
}

Object *Dict::getValNF(int i, Object *obj) {
  return entries[i].val.copy(obj);
}
