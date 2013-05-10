//========================================================================
//
// Parser.cc
//
// Copyright 1996 Derek B. Noonburg
//
//========================================================================

#ifdef __GNUC__
#pragma implementation
#endif

#include <stddef.h>
#include "Object.h"
#include "Array.h"
#include "Dict.h"
#include "Parser.h"
#include "Error.h"

Parser::Parser(Lexer *lexer1) {
  lexer = lexer1;
  inlineImg = 0;
  lexer->getObj(&buf1);
  lexer->getObj(&buf2);
}

Parser::~Parser() {
  buf1.free();
  buf2.free();
  delete lexer;
}

Object *Parser::getObj(Object *obj) {
  char *key;
  Stream *str;
  Object obj2;
  int num;

  // refill buffer after inline image data
  if (inlineImg == 2) {
    buf1.free();
    buf2.free();
    lexer->getObj(&buf1);
    lexer->getObj(&buf2);
    inlineImg = 0;
  }

  // array
  if (buf1.isCmd("[")) {
    shift();
    obj->initArray();
    while (!buf1.isCmd("]") && !buf1.isEOF())
      obj->arrayAdd(getObj(&obj2));
    if (buf1.isEOF())
      error(getPos(), "End of file inside array");
    shift();

  // dictionary or stream
  } else if (buf1.isCmd("<<")) {
    shift();
    obj->initDict();
    while (!buf1.isCmd(">>") && !buf1.isEOF()) {
      if (!buf1.isName()) {
	error(getPos(), "Dictionary key must be a name object");
	shift();
      } else {
	key = copyString(buf1.getName());
	shift();
	if (buf1.isEOF() || buf1.isError())
	  break;
	obj->dictAdd(key, getObj(&obj2));
      }
    }
    if (buf1.isEOF())
      error(getPos(), "End of file inside dictionary");
    if (buf2.isCmd("stream")) {
      if ((str = makeStream(obj))) {
	obj->initStream(str);
      } else {
	obj->free();
	obj->initError();
      }
    } else {
      shift();
    }

  // indirect reference or integer
  } else if (buf1.isInt()) {
    num = buf1.getInt();
    shift();
    if (buf1.isInt() && buf2.isCmd("R")) {
      obj->initRef(num, buf1.getInt());
      shift();
      shift();
    } else {
      obj->initInt(num);
    }

  // simple object
  } else {
    buf1.copy(obj);
    shift();
  }

  return obj;
}

Stream *Parser::makeStream(Object *dict) {
  Object obj;
  Stream *str;
  int pos, length;

  // get stream start position
  lexer->skipToNextLine();
  pos = lexer->getPos();

  // get length
  dict->dictLookup("Length", &obj);
  if (obj.isInt()) {
    length = obj.getInt();
    obj.free();
  } else {
    error(getPos(), "Bad 'Length' attribute in stream");
    obj.free();
    return NULL;
  }

  // make base stream
  str = new FileStream(lexer->getStream()->getFile(), pos, length, dict);

  // get filters
  str = str->addFilters(dict);

  // skip over stream data
  lexer->setPos(pos + length);

  // refill token buffers and check for 'endstream'
  shift();  // kill '>>'
  shift();  // kill 'stream'
  if (buf1.isCmd("endstream"))
    shift();
  else
    error(getPos(), "Missing 'endstream'");

  return str;
}

void Parser::shift() {
  if (inlineImg > 0) {
    ++inlineImg;
  } else if (buf2.isCmd("ID")) {
    lexer->skipChar();		// skip char after 'ID' command
    inlineImg = 1;
  }
  buf1.free();
  buf1 = buf2;
  if (inlineImg > 0)		// don't buffer inline image data
    buf2.initNull();
  else
    lexer->getObj(&buf2);
}
