//========================================================================
//
// Outline.cc
//
// Copyright 2002 Glyph & Cog, LLC
//
//========================================================================

#include <config.h>

#ifdef USE_GCC_PRAGMAS
#pragma implementation
#endif

#include "gmem.h"
#include "GString.h"
#include "GList.h"
#include "Link.h"
#include "PDFDocEncoding.h"
#include "Outline.h"

//------------------------------------------------------------------------

Outline::Outline(Object *outlineObj, XRef *xref) {
  Object first;

  items = NULL;
  if (!outlineObj->isDict()) {
    return;
  }
  items = OutlineItem::readItemList(outlineObj->dictLookupNF("First", &first),
				    xref);
  first.free();
}

Outline::~Outline() {
  if (items) {
    deleteGList(items, OutlineItem);
  }
}

//------------------------------------------------------------------------

OutlineItem::OutlineItem(Dict *dict, XRef *xrefA) {
  Object obj1;
  GString *s;
  int i;

  xref = xrefA;
  title = NULL;
  action = NULL;
  kids = NULL;

  if (dict->lookup("Title", &obj1)->isString()) {
    s = obj1.getString();
    if ((s->getChar(0) & 0xff) == 0xfe &&
	(s->getChar(1) & 0xff) == 0xff) {
      titleLen = (s->getLength() - 2) / 2;
      title = (Unicode *)gmalloc(titleLen * sizeof(Unicode));
      for (i = 0; i < titleLen; ++i) {
	title[i] = ((s->getChar(2 + 2*i) & 0xff) << 8) |
	           (s->getChar(3 + 2*i) & 0xff);
      }
    } else {
      titleLen = s->getLength();
      title = (Unicode *)gmalloc(titleLen * sizeof(Unicode));
      for (i = 0; i < titleLen; ++i) {
	title[i] = pdfDocEncoding[s->getChar(i) & 0xff];
      }
    }
  }
  obj1.free();

  if (!dict->lookup("Dest", &obj1)->isNull()) {
    action = LinkAction::parseDest(&obj1);
  } else {
    obj1.free();
    if (dict->lookup("A", &obj1)) {
      action = LinkAction::parseAction(&obj1);
    }
  }
  obj1.free();

  dict->lookupNF("First", &firstRef);
  dict->lookupNF("Next", &nextRef);

  startsOpen = gFalse;
  if (dict->lookup("Count", &obj1)->isInt()) {
    if (obj1.getInt() > 0) {
      startsOpen = gTrue;
    }
  }
  obj1.free();
}

OutlineItem::~OutlineItem() {
  close();
  if (title) {
    gfree(title);
  }
  if (action) {
    delete action;
  }
  firstRef.free();
  nextRef.free();
}

GList *OutlineItem::readItemList(Object *itemRef, XRef *xrefA) {
  GList *items;
  OutlineItem *item;
  Object obj;
  Object *p;

  items = new GList();
  p = itemRef;
  while (p->isRef()) {
    if (!p->fetch(xrefA, &obj)->isDict()) {
      obj.free();
      break;
    }
    item = new OutlineItem(obj.getDict(), xrefA);
    obj.free();
    items->append(item);
    p = &item->nextRef;
  }
  return items;
}

void OutlineItem::open() {
  if (!kids) {
    kids = readItemList(&firstRef, xref);
  }
}

void OutlineItem::close() {
  if (kids) {
    deleteGList(kids, OutlineItem);
    kids = NULL;
  }
}
