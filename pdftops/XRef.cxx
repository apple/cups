//========================================================================
//
// XRef.cc
//
// Copyright 1996 Derek B. Noonburg
//
//========================================================================

#ifdef __GNUC__
#pragma implementation
#endif

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
#include "Error.h"
#include "XRef.h"

//------------------------------------------------------------------------

#define xrefSearchSize 1024	// read this many bytes at end of file
				//   to look for 'startxref'

//------------------------------------------------------------------------
// The global xref table
//------------------------------------------------------------------------

XRef *xref = NULL;

//------------------------------------------------------------------------
// XRef
//------------------------------------------------------------------------

XRef::XRef(FileStream *str) {
  XRef *oldXref;
  int pos;
  int i;

  ok = gTrue;
  size = 0;
  entries = NULL;

  // get rid of old xref (otherwise it will try to fetch the Root object
  // in the new document, using the old xref)
  oldXref = xref;
  xref = NULL;

  // read the trailer
  file = str->getFile();
  start = str->getStart();
  pos = readTrailer(str);

  // if there was a problem with the trailer,
  // try to reconstruct the xref table
  if (pos == 0) {
    if (!(ok = constructXRef(str))) {
      xref = oldXref;
      return;
    }

  // trailer is ok - read the xref table
  } else {
    entries = (XRefEntry *)gmalloc(size * sizeof(XRefEntry));
    for (i = 0; i < size; ++i) {
      entries[i].offset = -1;
      entries[i].used = gFalse;
    }
    while (readXRef(str, &pos)) ;

    // if there was a problem with the xref table,
    // try to reconstruct it
    if (!ok) {
      gfree(entries);
      size = 0;
      entries = NULL;
      if (!(ok = constructXRef(str))) {
	xref = oldXref;
	return;
      }
    }
  }

  // set up new xref table
  xref = this;

  // check for encryption
  if (checkEncrypted()) {
    ok = gFalse;
    xref = oldXref;
    return;
  }
}

XRef::~XRef() {
  gfree(entries);
  trailerDict.free();
}

// Read startxref position, xref table size, and root.  Returns
// first xref position.
int XRef::readTrailer(FileStream *str) {
  Parser *parser;
  Object obj;
  char buf[xrefSearchSize+1];
  int n, pos, pos1;
  char *p;
  int c;
  int i;

  // read last xrefSearchSize bytes
  str->setPos(-xrefSearchSize);
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
  pos = atoi(p);

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
  parser = new Parser(new Lexer(new FileStream(file, start + pos1, -1, &obj)));
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
GBool XRef::readXRef(FileStream *str, int *pos) {
  Parser *parser;
  Object obj, obj2;
  char s[20];
  GBool more;
  int first, n, i, j;
  int c;

  // seek to xref in stream
  str->setPos(start + *pos);

  // make sure it's an xref table
  while ((c = str->getChar()) != EOF && isspace(c)) ;
  s[0] = (char)c;
  s[1] = (char)str->getChar();
  s[2] = (char)str->getChar();
  s[3] = (char)str->getChar();
  if (!(s[0] == 'x' && s[1] == 'r' && s[2] == 'e' && s[3] == 'f'))
    goto err2;

  // read xref
  while (1) {
    while ((c = str->lookChar()) != EOF && isspace(c))
      str->getChar();
    if (c == 't')
      break;
    for (i = 0; (c = str->getChar()) != EOF && isdigit(c) && i < 20; ++i)
      s[i] = (char)c;
    if (i == 0)
      goto err2;
    s[i] = '\0';
    first = atoi(s);
    while ((c = str->lookChar()) != EOF && isspace(c))
      str->getChar();
    for (i = 0; (c = str->getChar()) != EOF && isdigit(c) && i < 20; ++i)
      s[i] = (char)c;
    if (i == 0)
      goto err2;
    s[i] = '\0';
    n = atoi(s);
    while ((c = str->lookChar()) != EOF && isspace(c))
      str->getChar();
    for (i = first; i < first + n; ++i) {
      for (j = 0; j < 20; ++j) {
	if ((c = str->getChar()) == EOF)
	  goto err2;
	s[j] = (char)c;
      }
      if (entries[i].offset < 0) {
	s[10] = '\0';
	entries[i].offset = atoi(s);
	s[16] = '\0';
	entries[i].gen = atoi(&s[11]);
	if (s[17] == 'n')
	  entries[i].used = gTrue;
	else if (s[17] == 'f')
	  entries[i].used = gFalse;
	else
	  goto err2;
      }
    }
  }

  // read prev pointer from trailer dictionary
  obj.initNull();
  parser = new Parser(new Lexer(
    new FileStream(file, str->getPos(), -1, &obj)));
  parser->getObj(&obj);
  if (!obj.isCmd("trailer"))
    goto err1;
  obj.free();
  parser->getObj(&obj);
  if (!obj.isDict())
    goto err1;
  obj.getDict()->lookupNF("Prev", &obj2);
  if (obj2.isInt()) {
    *pos = obj2.getInt();
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
GBool XRef::constructXRef(FileStream *str) {
  Parser *parser;
  Object obj;
  char buf[256];
  int pos;
  int num, gen;
  int newSize;
  char *p;
  int i;
  GBool gotRoot;

  error(0, "PDF file is damaged - attempting to reconstruct xref table...");
  gotRoot = gFalse;

  str->reset();
  while (1) {
    pos = str->getPos();
    if (!str->getLine(buf, 256))
      break;
    p = buf;

    // got trailer dictionary
    if (!strncmp(p, "trailer", 7)) {
      obj.initNull();
      parser = new Parser(new Lexer(
		 new FileStream(file, start + pos + 8, -1, &obj)));
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
		  entries[i].offset = -1;
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
    }
  }

  if (gotRoot)
    return gTrue;

  error(-1, "Couldn't find trailer dictionary");
  return gFalse;
}

GBool XRef::checkEncrypted() {
  Object obj;
  GBool encrypted;

  trailerDict.dictLookup("Encrypt", &obj);
  if ((encrypted = !obj.isNull())) {
    error(-1, "PDF file is encrypted and cannot be displayed");
    error(-1, "* Decryption support is currently not included in xpdf");
    error(-1, "* due to legal restrictions: the U.S.A. still has bogus");
    error(-1, "* export controls on cryptography software.");
  }
  obj.free();
  return encrypted;
}

GBool XRef::okToPrint() {
  return gTrue;
}

GBool XRef::okToCopy() {
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
  if (e->gen == gen && e->offset >= 0) {
    obj1.initNull();
    parser = new Parser(new Lexer(
      new FileStream(file, start + e->offset, -1, &obj1)));
    parser->getObj(&obj1);
    parser->getObj(&obj2);
    parser->getObj(&obj3);
    if (obj1.isInt() && obj1.getInt() == num &&
	obj2.isInt() && obj2.getInt() == gen &&
	obj3.isCmd("obj")) {
      parser->getObj(obj);
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
