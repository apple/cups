//========================================================================
//
// GList.cc
//
// Copyright 2001-2002 Glyph & Cog, LLC
//
//========================================================================

#include <config.h>

#ifdef USE_GCC_PRAGMAS
#pragma implementation
#endif

#include <string.h>
#include "gmem.h"
#include "GList.h"

//------------------------------------------------------------------------
// GList
//------------------------------------------------------------------------

GList::GList() {
  size = 8;
  data = (void **)gmalloc(size * sizeof(void*));
  length = 0;
  inc = 0;
}

GList::GList(int sizeA) {
  size = sizeA;
  data = (void **)gmalloc(size * sizeof(void*));
  length = 0;
  inc = 0;
}

GList::~GList() {
  gfree(data);
}

void GList::append(void *p) {
  if (length >= size) {
    expand();
  }
  data[length++] = p;
}

void GList::append(GList *list) {
  int i;

  while (length + list->length > size) {
    expand();
  }
  for (i = 0; i < list->length; ++i) {
    data[length++] = list->data[i];
  }
}

void GList::insert(int i, void *p) {
  if (length >= size) {
    expand();
  }
  if (i < length) {
    memmove(data+i+1, data+i, (length - i) * sizeof(void *));
  }
  data[i] = p;
  ++length;
}

void *GList::del(int i) {
  void *p;

  p = data[i];
  if (i < length - 1) {
    memmove(data+i, data+i+1, (length - i - 1) * sizeof(void *));
  }
  --length;
  if (size - length >= ((inc > 0) ? inc : size/2)) {
    shrink();
  }
  return p;
}

void GList::expand() {
  size += (inc > 0) ? inc : size;
  data = (void **)grealloc(data, size * sizeof(void*));
}

void GList::shrink() {
  size -= (inc > 0) ? inc : size/2;
  data = (void **)grealloc(data, size * sizeof(void*));
}
