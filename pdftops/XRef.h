//========================================================================
//
// XRef.h
//
// Copyright 1996-2003 Glyph & Cog, LLC
//
//========================================================================

#ifndef XREF_H
#define XREF_H

#include <config.h>

#ifdef USE_GCC_PRAGMAS
#pragma interface
#endif

#include "gtypes.h"
#include "Object.h"

class Dict;
class Stream;

//------------------------------------------------------------------------
// XRef
//------------------------------------------------------------------------

struct XRefEntry {
  Guint offset;
  int gen;
  GBool used;
};

class XRef {
public:

  // Constructor.  Read xref table from stream.
  XRef(BaseStream *strA, GString *ownerPassword, GString *userPassword);

  // Destructor.
  ~XRef();

  // Is xref table valid?
  GBool isOk() { return ok; }

  // Get the error code (if isOk() returns false).
  int getErrorCode() { return errCode; }

  // Is the file encrypted?
#ifndef NO_DECRYPTION
  GBool isEncrypted() { return encrypted; }
#else
  GBool isEncrypted() { return gFalse; }
#endif

  // Check various permissions.
  GBool okToPrint(GBool ignoreOwnerPW = gFalse);
  GBool okToChange(GBool ignoreOwnerPW = gFalse);
  GBool okToCopy(GBool ignoreOwnerPW = gFalse);
  GBool okToAddNotes(GBool ignoreOwnerPW = gFalse);

  // Get catalog object.
  Object *getCatalog(Object *obj) { return fetch(rootNum, rootGen, obj); }

  // Fetch an indirect reference.
  Object *fetch(int num, int gen, Object *obj);

  // Return the document's Info dictionary (if any).
  Object *getDocInfo(Object *obj);
  Object *getDocInfoNF(Object *obj);

  // Return the number of objects in the xref table.
  int getNumObjects() { return size; }

  // Return the offset of the last xref table.
  Guint getLastXRefPos() { return lastXRefPos; }

  // Return the catalog object reference.
  int getRootNum() { return rootNum; }
  int getRootGen() { return rootGen; }

  // Get end position for a stream in a damaged file.
  // Returns false if unknown or file is not damaged.
  GBool getStreamEnd(Guint streamStart, Guint *streamEnd);

  // Direct access.
  int getSize() { return size; }
  XRefEntry *getEntry(int i) { return &entries[i]; }
  Object *getTrailerDict() { return &trailerDict; }

private:

  BaseStream *str;		// input stream
  Guint start;			// offset in file (to allow for garbage
				//   at beginning of file)
  XRefEntry *entries;		// xref entries
  int size;			// size of <entries> array
  int rootNum, rootGen;		// catalog dict
  GBool ok;			// true if xref table is valid
  int errCode;			// error code (if <ok> is false)
  Object trailerDict;		// trailer dictionary
  Guint lastXRefPos;		// offset of last xref table
  Guint *streamEnds;		// 'endstream' positions - only used in
				//   damaged files
  int streamEndsLen;		// number of valid entries in streamEnds
#ifndef NO_DECRYPTION
  GBool encrypted;		// true if file is encrypted
  int encVersion;		// encryption algorithm
  int encRevision;		// security handler revision
  int keyLength;		// length of key, in bytes
  int permFlags;		// permission bits
  Guchar fileKey[16];		// file decryption key
  GBool ownerPasswordOk;	// true if owner password is correct
#endif

  Guint readTrailer();
  GBool readXRef(Guint *pos);
  GBool constructXRef();
  GBool checkEncrypted(GString *ownerPassword, GString *userPassword);
  Guint strToUnsigned(char *s);
};

#endif
