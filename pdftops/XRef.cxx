//========================================================================
//
// XRef.cc
//
// Copyright 1996-2002 Glyph & Cog, LLC
//
//========================================================================

#ifdef __GNUC__
#pragma implementation
#endif

#include <config.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <ctype.h>
#include "gmem.h"
#include "Object.h"
#include "Stream.h"
#include "Lexer.h"
#include "Parser.h"
#include "Dict.h"
#ifndef NO_DECRYPTION
#include "Decrypt.h"
#endif
#include "Error.h"
#include "ErrorCodes.h"
#include "XRef.h"

//------------------------------------------------------------------------

#define xrefSearchSize 1024	// read this many bytes at end of file
				//   to look for 'startxref'

#ifndef NO_DECRYPTION
//------------------------------------------------------------------------
// Permission bits
//------------------------------------------------------------------------

#define permPrint    (1<<2)
#define permChange   (1<<3)
#define permCopy     (1<<4)
#define permNotes    (1<<5)
#define defPermFlags 0xfffc
#endif

//------------------------------------------------------------------------
// XRef
//------------------------------------------------------------------------

XRef::XRef(BaseStream *strA, GString *ownerPassword, GString *userPassword) {
  Guint pos;
  int i;

  ok = gTrue;
  errCode = errNone;
  size = 0;
  entries = NULL;
  streamEnds = NULL;
  streamEndsLen = 0;

  // read the trailer
  str = strA;
  start = str->getStart();
  pos = readTrailer();

  // if there was a problem with the trailer,
  // try to reconstruct the xref table
  if (pos == 0) {
    if (!(ok = constructXRef())) {
      errCode = errDamaged;
      return;
    }

  // trailer is ok - read the xref table
  } else {
    entries = (XRefEntry *)gmalloc(size * sizeof(XRefEntry));
    for (i = 0; i < size; ++i) {
      entries[i].offset = 0xffffffff;
      entries[i].used = gFalse;
    }
    while (readXRef(&pos)) ;

    // if there was a problem with the xref table,
    // try to reconstruct it
    if (!ok) {
      gfree(entries);
      size = 0;
      entries = NULL;
      if (!(ok = constructXRef())) {
	errCode = errDamaged;
	return;
      }
    }
  }

  // now set the trailer dictionary's xref pointer so we can fetch
  // indirect objects from it
  trailerDict.getDict()->setXRef(this);

  // check for encryption
#ifndef NO_DECRYPTION
  encrypted = gFalse;
#endif
  if (checkEncrypted(ownerPassword, userPassword)) {
    ok = gFalse;
    errCode = errEncrypted;
    return;
  }
}

XRef::~XRef() {
  gfree(entries);
  trailerDict.free();
  if (streamEnds) {
    gfree(streamEnds);
  }
}

// Read startxref position, xref table size, and root.  Returns
// first xref position.
Guint XRef::readTrailer() {
  Parser *parser;
  Object obj;
  char buf[xrefSearchSize+1];
  int n;
  Guint pos, pos1;
  char *p;
  int c;
  int i;

  // read last xrefSearchSize bytes
  str->setPos(xrefSearchSize, -1);
  for (n = 0; n < xrefSearchSize; ++n) {
    if ((c = str->getChar()) == EOF)
      break;
    buf[n] = c;
  }
  buf[n] = '\0';

  // find startxref
  for (i = n - 9; i >= 0; --i) {
    if (!strncmp(&buf[i], "startxref", 9))
      break;
  }
  if (i < 0)
    return 0;
  for (p = &buf[i+9]; isspace(*p); ++p) ;
  pos = lastXRefPos = strToUnsigned(p);

  // find trailer dict by looking after first xref table
  // (NB: we can't just use the trailer dict at the end of the file --
  // this won't work for linearized files.)
  str->setPos(start + pos);
  for (i = 0; i < 4; ++i)
    buf[i] = str->getChar();
  if (strncmp(buf, "xref", 4))
    return 0;
  pos1 = pos + 4;
  while (1) {
    str->setPos(start + pos1);
    for (i = 0; i < 35; ++i) {
      if ((c = str->getChar()) == EOF)
	return 0;
      buf[i] = c;
    }
    if (!strncmp(buf, "trailer", 7))
      break;
    p = buf;
    while (isspace(*p)) ++p;
    while ('0' <= *p && *p <= '9') ++p;
    while (isspace(*p)) ++p;
    n = atoi(p);
    while ('0' <= *p && *p <= '9') ++p;
    while (isspace(*p)) ++p;
    if (p == buf)
      return 0;
    pos1 += (p - buf) + n * 20;
  }
  pos1 += 7;

  // read trailer dict
  obj.initNull();
  parser = new Parser(NULL,
	     new Lexer(NULL,
	       str->makeSubStream(start + pos1, gFalse, 0, &obj)));
  parser->getObj(&trailerDict);
  if (trailerDict.isDict()) {
    trailerDict.dictLookupNF("Size", &obj);
    if (obj.isInt())
      size = obj.getInt();
    else
      pos = 0;
    obj.free();
    trailerDict.dictLookupNF("Root", &obj);
    if (obj.isRef()) {
      rootNum = obj.getRefNum();
      rootGen = obj.getRefGen();
    } else {
      pos = 0;
    }
    obj.free();
  } else {
    pos = 0;
  }
  delete parser;

  // return first xref position
  return pos;
}

// Read an xref table and the prev pointer from the trailer.
GBool XRef::readXRef(Guint *pos) {
  Parser *parser;
  Object obj, obj2;
  char s[20];
  GBool more;
  int first, newSize, n, i, j;
  int c;

  // seek to xref in stream
  str->setPos(start + *pos);

  // make sure it's an xref table
  while ((c = str->getChar()) != EOF && isspace(c)) ;
  s[0] = (char)c;
  s[1] = (char)str->getChar();
  s[2] = (char)str->getChar();
  s[3] = (char)str->getChar();
  if (!(s[0] == 'x' && s[1] == 'r' && s[2] == 'e' && s[3] == 'f')) {
    goto err2;
  }

  // read xref
  while (1) {
    while ((c = str->lookChar()) != EOF && isspace(c)) {
      str->getChar();
    }
    if (c == 't') {
      break;
    }
    for (i = 0; (c = str->getChar()) != EOF && isdigit(c) && i < 20; ++i) {
      s[i] = (char)c;
    }
    if (i == 0) {
      goto err2;
    }
    s[i] = '\0';
    first = atoi(s);
    while ((c = str->lookChar()) != EOF && isspace(c)) {
      str->getChar();
    }
    for (i = 0; (c = str->getChar()) != EOF && isdigit(c) && i < 20; ++i) {
      s[i] = (char)c;
    }
    if (i == 0) {
      goto err2;
    }
    s[i] = '\0';
    n = atoi(s);
    while ((c = str->lookChar()) != EOF && isspace(c)) {
      str->getChar();
    }
    // check for buggy PDF files with an incorrect (too small) xref
    // table size
    if (first + n > size) {
      newSize = size + 256;
      entries = (XRefEntry *)grealloc(entries, newSize * sizeof(XRefEntry));
      for (i = size; i < newSize; ++i) {
	entries[i].offset = 0xffffffff;
	entries[i].used = gFalse;
      }
      size = newSize;
    }
    for (i = first; i < first + n; ++i) {
      for (j = 0; j < 20; ++j) {
	if ((c = str->getChar()) == EOF) {
	  goto err2;
	}
	s[j] = (char)c;
      }
      if (entries[i].offset == 0xffffffff) {
	s[10] = '\0';
	entries[i].offset = strToUnsigned(s);
	s[16] = '\0';
	entries[i].gen = atoi(&s[11]);
	if (s[17] == 'n') {
	  entries[i].used = gTrue;
	} else if (s[17] == 'f') {
	  entries[i].used = gFalse;
	} else {
	  goto err2;
	}
	// PDF files of patents from the IBM Intellectual Property
	// Network have a bug: the xref table claims to start at 1
	// instead of 0.
	if (i == 1 && first == 1 &&
	    entries[1].offset == 0 && entries[1].gen == 65535 &&
	    !entries[1].used) {
	  i = first = 0;
	  entries[0] = entries[1];
	  entries[1].offset = 0xffffffff;
	}
      }
    }
  }

  // read prev pointer from trailer dictionary
  obj.initNull();
  parser = new Parser(NULL,
	     new Lexer(NULL,
	       str->makeSubStream(str->getPos(), gFalse, 0, &obj)));
  parser->getObj(&obj);
  if (!obj.isCmd("trailer")) {
    goto err1;
  }
  obj.free();
  parser->getObj(&obj);
  if (!obj.isDict()) {
    goto err1;
  }
  obj.getDict()->lookupNF("Prev", &obj2);
  if (obj2.isInt()) {
    *pos = (Guint)obj2.getInt();
    more = gTrue;
  } else {
    more = gFalse;
  }
  obj.free();
  obj2.free();

  delete parser;
  return more;

 err1:
  obj.free();
 err2:
  ok = gFalse;
  return gFalse;
}

// Attempt to construct an xref table for a damaged file.
GBool XRef::constructXRef() {
  Parser *parser;
  Object obj;
  char buf[256];
  Guint pos;
  int num, gen;
  int newSize;
  int streamEndsSize;
  char *p;
  int i;
  GBool gotRoot;

  error(0, "PDF file is damaged - attempting to reconstruct xref table...");
  gotRoot = gFalse;
  streamEndsLen = streamEndsSize = 0;

  str->reset();
  while (1) {
    pos = str->getPos();
    if (!str->getLine(buf, 256)) {
      break;
    }
    p = buf;

    // got trailer dictionary
    if (!strncmp(p, "trailer", 7)) {
      obj.initNull();
      parser = new Parser(NULL,
		 new Lexer(NULL,
		   str->makeSubStream(start + pos + 7, gFalse, 0, &obj)));
      if (!trailerDict.isNone())
	trailerDict.free();
      parser->getObj(&trailerDict);
      if (trailerDict.isDict()) {
	trailerDict.dictLookupNF("Root", &obj);
	if (obj.isRef()) {
	  rootNum = obj.getRefNum();
	  rootGen = obj.getRefGen();
	  gotRoot = gTrue;
	}
	obj.free();
      } else {
	pos = 0;
      }
      delete parser;

    // look for object
    } else if (isdigit(*p)) {
      num = atoi(p);
      do {
	++p;
      } while (*p && isdigit(*p));
      if (isspace(*p)) {
	do {
	  ++p;
	} while (*p && isspace(*p));
	if (isdigit(*p)) {
	  gen = atoi(p);
	  do {
	    ++p;
	  } while (*p && isdigit(*p));
	  if (isspace(*p)) {
	    do {
	      ++p;
	    } while (*p && isspace(*p));
	    if (!strncmp(p, "obj", 3)) {
	      if (num >= size) {
		newSize = (num + 1 + 255) & ~255;
		entries = (XRefEntry *)
		            grealloc(entries, newSize * sizeof(XRefEntry));
		for (i = size; i < newSize; ++i) {
		  entries[i].offset = 0xffffffff;
		  entries[i].used = gFalse;
		}
		size = newSize;
	      }
	      if (!entries[num].used || gen >= entries[num].gen) {
		entries[num].offset = pos - start;
		entries[num].gen = gen;
		entries[num].used = gTrue;
	      }
	    }
	  }
	}
      }

    } else if (!strncmp(p, "endstream", 9)) {
      if (streamEndsLen == streamEndsSize) {
	streamEndsSize += 64;
	streamEnds = (Guint *)grealloc(streamEnds,
				       streamEndsSize * sizeof(int));
      }
      streamEnds[streamEndsLen++] = pos;
    }
  }

  if (gotRoot)
    return gTrue;

  error(-1, "Couldn't find trailer dictionary");
  return gFalse;
}

#ifndef NO_DECRYPTION
GBool XRef::checkEncrypted(GString *ownerPassword, GString *userPassword) {
  Object encrypt, filterObj, versionObj, revisionObj, lengthObj;
  Object ownerKey, userKey, permissions, fileID, fileID1;
  GBool encrypted1;
  GBool ret;

  ret = gFalse;

  permFlags = defPermFlags;
  trailerDict.dictLookup("Encrypt", &encrypt);
  if ((encrypted1 = encrypt.isDict())) {
    ret = gTrue;
    encrypt.dictLookup("Filter", &filterObj);
    if (filterObj.isName("Standard")) {
      encrypt.dictLookup("V", &versionObj);
      encrypt.dictLookup("R", &revisionObj);
      encrypt.dictLookup("Length", &lengthObj);
      encrypt.dictLookup("O", &ownerKey);
      encrypt.dictLookup("U", &userKey);
      encrypt.dictLookup("P", &permissions);
      trailerDict.dictLookup("ID", &fileID);
      if (versionObj.isInt() &&
	  revisionObj.isInt() &&
	  ownerKey.isString() && ownerKey.getString()->getLength() == 32 &&
	  userKey.isString() && userKey.getString()->getLength() == 32 &&
	  permissions.isInt() &&
	  fileID.isArray()) {
	encVersion = versionObj.getInt();
	encRevision = revisionObj.getInt();
	if (lengthObj.isInt()) {
	  keyLength = lengthObj.getInt() / 8;
	} else {
	  keyLength = 5;
	}
	permFlags = permissions.getInt();
	if (encVersion >= 1 && encVersion <= 2 &&
	    encRevision >= 2 && encRevision <= 3) {
	  fileID.arrayGet(0, &fileID1);
	  if (fileID1.isString()) {
	    if (Decrypt::makeFileKey(encVersion, encRevision, keyLength,
				     ownerKey.getString(), userKey.getString(),
				     permFlags, fileID1.getString(),
				     ownerPassword, userPassword, fileKey,
				     &ownerPasswordOk)) {
	      if (ownerPassword && !ownerPasswordOk) {
		error(-1, "Incorrect owner password");
	      }
	      ret = gFalse;
	    } else {
	      error(-1, "Incorrect password");
	    }
	  } else {
	    error(-1, "Weird encryption info");
	  }
	  fileID1.free();
	} else {
	  error(-1, "Unsupported version/revision (%d/%d) of Standard security handler",
		encVersion, encRevision);
	}
      } else {
	error(-1, "Weird encryption info");
      }
      fileID.free();
      permissions.free();
      userKey.free();
      ownerKey.free();
      lengthObj.free();
      revisionObj.free();
      versionObj.free();
    } else {
      error(-1, "Unknown security handler '%s'",
	    filterObj.isName() ? filterObj.getName() : "???");
    }
    filterObj.free();
  }
  encrypt.free();

  // this flag has to be set *after* we read the O/U/P strings
  encrypted = encrypted1;

  return ret;
}
#else
GBool XRef::checkEncrypted(GString *ownerPassword, GString *userPassword) {
  Object obj;
  GBool encrypted;

  trailerDict.dictLookup("Encrypt", &obj);
  if ((encrypted = !obj.isNull())) {
    error(-1, "PDF file is encrypted and this version of the Xpdf tools");
    error(-1, "was built without decryption support.");
  }
  obj.free();
  return encrypted;
}
#endif

GBool XRef::okToPrint(GBool ignoreOwnerPW) {
#ifndef NO_DECRYPTION
  if ((ignoreOwnerPW || !ownerPasswordOk) && !(permFlags & permPrint)) {
    return gFalse;
  }
#endif
  return gTrue;
}

GBool XRef::okToChange(GBool ignoreOwnerPW) {
#ifndef NO_DECRYPTION
  if ((ignoreOwnerPW || !ownerPasswordOk) && !(permFlags & permChange)) {
    return gFalse;
  }
#endif
  return gTrue;
}

GBool XRef::okToCopy(GBool ignoreOwnerPW) {
#ifndef NO_DECRYPTION
  if ((ignoreOwnerPW || !ownerPasswordOk) && !(permFlags & permCopy)) {
    return gFalse;
  }
#endif
  return gTrue;
}

GBool XRef::okToAddNotes(GBool ignoreOwnerPW) {
#ifndef NO_DECRYPTION
  if ((ignoreOwnerPW || !ownerPasswordOk) && !(permFlags & permNotes)) {
    return gFalse;
  }
#endif
  return gTrue;
}

Object *XRef::fetch(int num, int gen, Object *obj) {
  XRefEntry *e;
  Parser *parser;
  Object obj1, obj2, obj3;

  // check for bogus ref - this can happen in corrupted PDF files
  if (num < 0 || num >= size) {
    obj->initNull();
    return obj;
  }

  e = &entries[num];
  if (e->gen == gen && e->offset != 0xffffffff) {
    obj1.initNull();
    parser = new Parser(this,
	       new Lexer(this,
		 str->makeSubStream(start + e->offset, gFalse, 0, &obj1)));
    parser->getObj(&obj1);
    parser->getObj(&obj2);
    parser->getObj(&obj3);
    if (obj1.isInt() && obj1.getInt() == num &&
	obj2.isInt() && obj2.getInt() == gen &&
	obj3.isCmd("obj")) {
#ifndef NO_DECRYPTION
      parser->getObj(obj, encrypted ? fileKey : (Guchar *)NULL, keyLength,
		     num, gen);
#else
      parser->getObj(obj);
#endif
    } else {
      obj->initNull();
    }
    obj1.free();
    obj2.free();
    obj3.free();
    delete parser;
  } else {
    obj->initNull();
  }
  return obj;
}

Object *XRef::getDocInfo(Object *obj) {
  return trailerDict.dictLookup("Info", obj);
}

// Added for the pdftex project.
Object *XRef::getDocInfoNF(Object *obj) {
  return trailerDict.dictLookupNF("Info", obj);
}

GBool XRef::getStreamEnd(Guint streamStart, Guint *streamEnd) {
  int a, b, m;

  if (streamEndsLen == 0 ||
      streamStart > streamEnds[streamEndsLen - 1]) {
    return gFalse;
  }

  a = -1;
  b = streamEndsLen - 1;
  // invariant: streamEnds[a] < streamStart <= streamEnds[b]
  while (b - a > 1) {
    m = (a + b) / 2;
    if (streamStart <= streamEnds[m]) {
      b = m;
    } else {
      a = m;
    }
  }
  *streamEnd = streamEnds[b];
  return gTrue;
}

Guint XRef::strToUnsigned(char *s) {
  Guint x;
  char *p;
  int i;

  x = 0;
  for (p = s, i = 0; *p && isdigit(*p) && i < 10; ++p, ++i) {
    x = 10 * x + (*p - '0');
  }
  return x;
}
