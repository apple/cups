//========================================================================
//
// GHash.h
//
// Copyright 2001-2002 Glyph & Cog, LLC
//
//========================================================================

#ifndef GHASH_H
#define GHASH_H

#ifdef __GNUC__
#pragma interface
#endif

#include "gtypes.h"

class GString;
struct GHashBucket;
struct GHashIter;

//------------------------------------------------------------------------

class GHash {
public:

  GHash(GBool deleteKeysA = gFalse);
  ~GHash();
  void add(GString *key, void *val);
  void *lookup(GString *key);
  void *lookup(char *key);
  void *remove(GString *key);
  void *remove(char *key);
  int getLength() { return len; }
  void startIter(GHashIter **iter);
  GBool getNext(GHashIter **iter, GString **key, void **val);
  void killIter(GHashIter **iter);

private:

  GHashBucket *find(GString *key, int *h);
  GHashBucket *find(char *key, int *h);
  int hash(GString *key);
  int hash(char *key);

  GBool deleteKeys;		// set if key strings should be deleted
  int size;			// number of buckets
  int len;			// number of entries
  GHashBucket **tab;
};

#define deleteGHash(hash, T)                       \
  do {                                             \
    GHash *_hash = (hash);                         \
    {                                              \
      GHashIter *_iter;                            \
      GString *_key;                               \
      void *_p;                                    \
      _hash->startIter(&_iter);                    \
      while (_hash->getNext(&_iter, &_key, &_p)) { \
        delete (T*)_p;                             \
      }                                            \
      delete _hash;                                \
    }                                              \
  } while(0)

#endif
