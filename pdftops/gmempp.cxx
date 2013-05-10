//========================================================================
//
// gmempp.cc
//
// Use gmalloc/gfree for C++ new/delete operators.
//
// Copyright 1996 Derek B. Noonburg
//
//========================================================================

#include "gmem.h"

#ifdef DEBUG_MEM
void *operator new(long size) {
  return gmalloc((int)size);
}
#endif

#ifdef DEBUG_MEM
void operator delete(void *p) {
  gfree(p);
}
#endif
