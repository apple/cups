//========================================================================
//
// Dict.cc
//
// Copyright 1996 Derek B. Noonburg
//
//========================================================================

#ifdef __GNUC__
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

Dict::Dict() {
  entries = NULL;
  size = length = 0;
  ref = 1;
}

Dict::~Dict() {
  int i;

  for (i = 0; i < length; ++i) {
    gfree(entries[i].key);
    entries[i].val.free();
  }
  gfree(entries);
}

void Dict::add(char *key, Object *val) {
  if (length + 1 > size) {
    size += 8;
    entries = (DictEntry *)grealloc(entries, size * sizeof(DictEntry));
  }
  entries[length].key = key;
  entries[length].val = *val;
  ++length;
}

inline DictEntry *Dict::find(char *key) {
  int i;

  for (i = 0; i < length; ++i) {
    if (!strcmp(key, entries[i].key))
      return &entries[i];
  }
  return NULL;
}

GBool Dict::is(char *type) {
  DictEntry *e;

  return (e = find("Type")) && e->val.isName(type);
}

Object *Dict::lookup(char *key, Object *obj) {
  DictEntry *e;

  return (e = find(key)) ? e->val.fetch(obj) : obj->initNull();
}

Object *Dict::lookupNF(char *key, Object *obj) {
  DictEntry *e;

  return (e = find(key)) ? e->val.copy(obj) : obj->initNull();
}

char *Dict::getKey(int i) {
  return entries[i].key;
}

Object *Dict::getVal(int i, Object *obj) {
  return entries[i].val.fetch(obj);
}

Object *Dict::getValNF(int i, Object *obj) {
  return entries[i].val.copy(obj);
}
