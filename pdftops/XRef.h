//========================================================================
//
// XRef.h
//
// Copyright 1996 Derek B. Noonburg
//
//========================================================================

#ifndef XREF_H
#define XREF_H

#ifdef __GNUC__
#pragma interface
#endif

#include <stdio.h>
#include "gtypes.h"
#include "Object.h"

class Dict;
class FileStream;

//------------------------------------------------------------------------
// XRef
//------------------------------------------------------------------------

struct XRefEntry {
  int offset;
  int gen;
  GBool used;
};

class XRef {
public:

  // Constructor.  Read xref table from stream.
  XRef(FileStream *str);

  // Destructor.
  ~XRef();

  // Is xref table valid?
  GBool isOk() { return ok; }

  // Is the file encrypted?
  GBool isEncrypted() { return gFalse; }

  // Are printing and copying allowed?  If not, print an error message.
  GBool okToPrint();
  GBool okToCopy();

  // Get catalog object.
  Object *getCatalog(Object *obj) { return fetch(rootNum, rootGen, obj); }

  // Fetch an indirect reference.
  Object *fetch(int num, int gen, Object *obj);

  // Return the document's Info dictionary (if any).
  Object *getDocInfo(Object *obj);

private:

  FILE *file;			// input file
  int start;			// offset in file (to allow for garbage
				//   at beginning of file)
  XRefEntry *entries;		// xref entries
  int size;			// size of <entries> array
  int rootNum, rootGen;		// catalog dict
  GBool ok;			// true if xref table is valid
  Object trailerDict;		// trailer dictionary

  int readTrailer(FileStream *str);
  GBool readXRef(FileStream *str, int *pos);
  GBool constructXRef(FileStream *str);
  GBool checkEncrypted();
};

//------------------------------------------------------------------------
// The global xref table
//------------------------------------------------------------------------

extern XRef *xref;

#endif
