//========================================================================
//
// Link.cc
//
// Copyright 1996-2002 Glyph & Cog, LLC
//
//========================================================================

#ifdef __GNUC__
#pragma implementation
#endif

#include <config.h>
#include <stddef.h>
#include <string.h>
#include "gmem.h"
#include "GString.h"
#include "Error.h"
#include "Object.h"
#include "Array.h"
#include "Dict.h"
#include "Link.h"

//------------------------------------------------------------------------

static GString *getFileSpecName(Object *fileSpecObj);

//------------------------------------------------------------------------
// LinkDest
//------------------------------------------------------------------------

LinkDest::LinkDest(Array *a) {
  Object obj1, obj2;

  // initialize fields
  left = bottom = right = top = zoom = 0;
  ok = gFalse;

  // get page
  a->getNF(0, &obj1);
  if (obj1.isInt()) {
    pageNum = obj1.getInt() + 1;
    pageIsRef = gFalse;
  } else if (obj1.isRef()) {
    pageRef.num = obj1.getRefNum();
    pageRef.gen = obj1.getRefGen();
    pageIsRef = gTrue;
  } else {
    error(-1, "Bad annotation destination");
    goto err2;
  }
  obj1.free();

  // get destination type
  a->get(1, &obj1);

  // XYZ link
  if (obj1.isName("XYZ")) {
    kind = destXYZ;
    a->get(2, &obj2);
    if (obj2.isNull()) {
      changeLeft = gFalse;
    } else if (obj2.isNum()) {
      changeLeft = gTrue;
      left = obj2.getNum();
    } else {
      error(-1, "Bad annotation destination position");
      goto err1;
    }
    obj2.free();
    a->get(3, &obj2);
    if (obj2.isNull()) {
      changeTop = gFalse;
    } else if (obj2.isNum()) {
      changeTop = gTrue;
      top = obj2.getNum();
    } else {
      error(-1, "Bad annotation destination position");
      goto err1;
    }
    obj2.free();
    a->get(4, &obj2);
    if (obj2.isNull()) {
      changeZoom = gFalse;
    } else if (obj2.isNum()) {
      changeZoom = gTrue;
      zoom = obj2.getNum();
    } else {
      error(-1, "Bad annotation destination position");
      goto err1;
    }
    obj2.free();

  // Fit link
  } else if (obj1.isName("Fit")) {
    kind = destFit;

  // FitH link
  } else if (obj1.isName("FitH")) {
    kind = destFitH;
    if (!a->get(2, &obj2)->isNum()) {
      error(-1, "Bad annotation destination position");
      goto err1;
    }
    top = obj2.getNum();
    obj2.free();

  // FitV link
  } else if (obj1.isName("FitV")) {
    kind = destFitV;
    if (!a->get(2, &obj2)->isNum()) {
      error(-1, "Bad annotation destination position");
      goto err1;
    }
    left = obj2.getNum();
    obj2.free();

  // FitR link
  } else if (obj1.isName("FitR")) {
    kind = destFitR;
    if (!a->get(2, &obj2)->isNum()) {
      error(-1, "Bad annotation destination position");
      goto err1;
    }
    left = obj2.getNum();
    obj2.free();
    if (!a->get(3, &obj2)->isNum()) {
      error(-1, "Bad annotation destination position");
      goto err1;
    }
    bottom = obj2.getNum();
    obj2.free();
    if (!a->get(4, &obj2)->isNum()) {
      error(-1, "Bad annotation destination position");
      goto err1;
    }
    right = obj2.getNum();
    obj2.free();
    if (!a->get(5, &obj2)->isNum()) {
      error(-1, "Bad annotation destination position");
      goto err1;
    }
    top = obj2.getNum();
    obj2.free();

  // FitB link
  } else if (obj1.isName("FitB")) {
    kind = destFitB;

  // FitBH link
  } else if (obj1.isName("FitBH")) {
    kind = destFitBH;
    if (!a->get(2, &obj2)->isNum()) {
      error(-1, "Bad annotation destination position");
      goto err1;
    }
    top = obj2.getNum();
    obj2.free();

  // FitBV link
  } else if (obj1.isName("FitBV")) {
    kind = destFitBV;
    if (!a->get(2, &obj2)->isNum()) {
      error(-1, "Bad annotation destination position");
      goto err1;
    }
    left = obj2.getNum();
    obj2.free();

  // unknown link kind
  } else {
    error(-1, "Unknown annotation destination type");
    goto err2;
  }

  obj1.free();
  ok = gTrue;
  return;

 err1:
  obj2.free();
 err2:
  obj1.free();
}

LinkDest::LinkDest(LinkDest *dest) {
  kind = dest->kind;
  pageIsRef = dest->pageIsRef;
  if (pageIsRef)
    pageRef = dest->pageRef;
  else
    pageNum = dest->pageNum;
  left = dest->left;
  bottom = dest->bottom;
  right = dest->right;
  top = dest->top;
  zoom = dest->zoom;
  changeLeft = dest->changeLeft;
  changeTop = dest->changeTop;
  changeZoom = dest->changeZoom;
  ok = gTrue;
}

//------------------------------------------------------------------------
// LinkGoTo
//------------------------------------------------------------------------

LinkGoTo::LinkGoTo(Object *destObj) {
  dest = NULL;
  namedDest = NULL;

  // named destination
  if (destObj->isName()) {
    namedDest = new GString(destObj->getName());
  } else if (destObj->isString()) {
    namedDest = destObj->getString()->copy();

  // destination dictionary
  } else if (destObj->isArray()) {
    dest = new LinkDest(destObj->getArray());
    if (!dest->isOk()) {
      delete dest;
      dest = NULL;
    }

  // error
  } else {
    error(-1, "Illegal annotation destination");
  }
}

LinkGoTo::~LinkGoTo() {
  if (dest)
    delete dest;
  if (namedDest)
    delete namedDest;
}

//------------------------------------------------------------------------
// LinkGoToR
//------------------------------------------------------------------------

LinkGoToR::LinkGoToR(Object *fileSpecObj, Object *destObj) {
  dest = NULL;
  namedDest = NULL;

  // get file name
  fileName = getFileSpecName(fileSpecObj);

  // named destination
  if (destObj->isName()) {
    namedDest = new GString(destObj->getName());
  } else if (destObj->isString()) {
    namedDest = destObj->getString()->copy();

  // destination dictionary
  } else if (destObj->isArray()) {
    dest = new LinkDest(destObj->getArray());
    if (!dest->isOk()) {
      delete dest;
      dest = NULL;
    }

  // error
  } else {
    error(-1, "Illegal annotation destination");
  }
}

LinkGoToR::~LinkGoToR() {
  if (fileName)
    delete fileName;
  if (dest)
    delete dest;
  if (namedDest)
    delete namedDest;
}


//------------------------------------------------------------------------
// LinkLaunch
//------------------------------------------------------------------------

LinkLaunch::LinkLaunch(Object *actionObj) {
  Object obj1, obj2;

  fileName = NULL;
  params = NULL;

  if (actionObj->isDict()) {
    if (!actionObj->dictLookup("F", &obj1)->isNull()) {
      fileName = getFileSpecName(&obj1);
    } else {
      obj1.free();
      //~ This hasn't been defined by Adobe yet, so assume it looks
      //~ just like the Win dictionary until they say otherwise.
      if (actionObj->dictLookup("Unix", &obj1)->isDict()) {
	obj1.dictLookup("F", &obj2);
	fileName = getFileSpecName(&obj2);
	obj2.free();
	if (obj1.dictLookup("P", &obj2)->isString())
	  params = obj2.getString()->copy();
	obj2.free();
      } else {
	error(-1, "Bad launch-type link action");
      }
    }
    obj1.free();
  }
}

LinkLaunch::~LinkLaunch() {
  if (fileName)
    delete fileName;
  if (params)
    delete params;
}

//------------------------------------------------------------------------
// LinkURI
//------------------------------------------------------------------------

LinkURI::LinkURI(Object *uriObj, GString *baseURI) {
  GString *uri2;
  int n;
  char c;

  uri = NULL;
  if (uriObj->isString()) {
    uri2 = uriObj->getString()->copy();
    if (baseURI) {
      n = strcspn(uri2->getCString(), "/:");
      if (n == uri2->getLength() || uri2->getChar(n) == '/') {
	uri = baseURI->copy();
	c = uri->getChar(uri->getLength() - 1);
	if (c == '/' || c == '?') {
	  if (uri2->getChar(0) == '/') {
	    uri2->del(0);
	  }
	} else {
	  if (uri2->getChar(0) != '/') {
	    uri->append('/');
	  }
	}
	uri->append(uri2);
	delete uri2;
      } else {
	uri = uri2;
      }
    } else {
      uri = uri2;
    }
  } else {
    error(-1, "Illegal URI-type link");
  }
}

LinkURI::~LinkURI() {
  if (uri)
    delete uri;
}

//------------------------------------------------------------------------
// LinkNamed
//------------------------------------------------------------------------

LinkNamed::LinkNamed(Object *nameObj) {
  name = NULL;
  if (nameObj->isName()) {
    name = new GString(nameObj->getName());
  }
}

LinkNamed::~LinkNamed() {
  if (name) {
    delete name;
  }
}

//------------------------------------------------------------------------
// LinkUnknown
//------------------------------------------------------------------------

LinkUnknown::LinkUnknown(char *actionA) {
  action = new GString(actionA);
}

LinkUnknown::~LinkUnknown() {
  delete action;
}

//------------------------------------------------------------------------
// Link
//------------------------------------------------------------------------

Link::Link(Dict *dict, GString *baseURI) {
  Object obj1, obj2, obj3, obj4;
  double t;

  action = NULL;
  ok = gFalse;

  // get rectangle
  if (!dict->lookup("Rect", &obj1)->isArray()) {
    error(-1, "Annotation rectangle is wrong type");
    goto err2;
  }
  if (!obj1.arrayGet(0, &obj2)->isNum()) {
    error(-1, "Bad annotation rectangle");
    goto err1;
  }
  x1 = obj2.getNum();
  obj2.free();
  if (!obj1.arrayGet(1, &obj2)->isNum()) {
    error(-1, "Bad annotation rectangle");
    goto err1;
  }
  y1 = obj2.getNum();
  obj2.free();
  if (!obj1.arrayGet(2, &obj2)->isNum()) {
    error(-1, "Bad annotation rectangle");
    goto err1;
  }
  x2 = obj2.getNum();
  obj2.free();
  if (!obj1.arrayGet(3, &obj2)->isNum()) {
    error(-1, "Bad annotation rectangle");
    goto err1;
  }
  y2 = obj2.getNum();
  obj2.free();
  obj1.free();
  if (x1 > x2) {
    t = x1;
    x1 = x2;
    x2 = t;
  }
  if (y1 > y2) {
    t = y1;
    y1 = y2;
    y2 = t;
  }

  // get border
  borderW = 1;
  if (!dict->lookup("Border", &obj1)->isNull()) {
    if (obj1.isArray() && obj1.arrayGetLength() >= 3) {
      if (obj1.arrayGet(2, &obj2)->isNum()) {
	borderW = obj2.getNum();
      } else {
	error(-1, "Bad annotation border");
      }
      obj2.free();
    }
  }
  obj1.free();

  // look for destination
  if (!dict->lookup("Dest", &obj1)->isNull()) {
    action = new LinkGoTo(&obj1);

  // look for action
  } else {
    obj1.free();
    if (dict->lookup("A", &obj1)->isDict()) {
      obj1.dictLookup("S", &obj2);

      // GoTo action
      if (obj2.isName("GoTo")) {
	obj1.dictLookup("D", &obj3);
	action = new LinkGoTo(&obj3);
	obj3.free();

      // GoToR action
      } else if (obj2.isName("GoToR")) {
	obj1.dictLookup("F", &obj3);
	obj1.dictLookup("D", &obj4);
	action = new LinkGoToR(&obj3, &obj4);
	obj3.free();
	obj4.free();

      // Launch action
      } else if (obj2.isName("Launch")) {
	action = new LinkLaunch(&obj1);

      // URI action
      } else if (obj2.isName("URI")) {
	obj1.dictLookup("URI", &obj3);
	action = new LinkURI(&obj3, baseURI);
	obj3.free();

      // Named action
      } else if (obj2.isName("Named")) {
	obj1.dictLookup("N", &obj3);
	action = new LinkNamed(&obj3);
	obj3.free();

      // unknown action
      } else if (obj2.isName()) {
	action = new LinkUnknown(obj2.getName());

      // action is missing or wrong type
      } else {
	error(-1, "Bad annotation action");
	action = NULL;
      }

      obj2.free();

    } else {
      error(-1, "Missing annotation destination/action");
      action = NULL;
    }
  }
  obj1.free();

  // check for bad action
  if (action && action->isOk())
    ok = gTrue;

  return;

 err1:
  obj2.free();
 err2:
  obj1.free();
}

Link::~Link() {
  if (action)
    delete action;
}

//------------------------------------------------------------------------
// Links
//------------------------------------------------------------------------

Links::Links(Object *annots, GString *baseURI) {
  Link *link;
  Object obj1, obj2;
  int size;
  int i;

  links = NULL;
  size = 0;
  numLinks = 0;

  if (annots->isArray()) {
    for (i = 0; i < annots->arrayGetLength(); ++i) {
      if (annots->arrayGet(i, &obj1)->isDict()) {
	if (obj1.dictLookup("Subtype", &obj2)->isName("Link")) {
	  link = new Link(obj1.getDict(), baseURI);
	  if (link->isOk()) {
	    if (numLinks >= size) {
	      size += 16;
	      links = (Link **)grealloc(links, size * sizeof(Link *));
	    }
	    links[numLinks++] = link;
	  } else {
	    delete link;
	  }
	}
	obj2.free();
      }
      obj1.free();
    }
  }
}

Links::~Links() {
  int i;

  for (i = 0; i < numLinks; ++i)
    delete links[i];
  gfree(links);
}

LinkAction *Links::find(double x, double y) {
  int i;

  for (i = numLinks - 1; i >= 0; --i) {
    if (links[i]->inRect(x, y)) {
      return links[i]->getAction();
    }
  }
  return NULL;
}

GBool Links::onLink(double x, double y) {
  int i;

  for (i = 0; i < numLinks; ++i) {
    if (links[i]->inRect(x, y))
      return gTrue;
  }
  return gFalse;
}

//------------------------------------------------------------------------

// Extract a file name from a file specification (string or dictionary).
static GString *getFileSpecName(Object *fileSpecObj) {
  GString *name;
  Object obj1;

  name = NULL;

  // string
  if (fileSpecObj->isString()) {
    name = fileSpecObj->getString()->copy();

  // dictionary
  } else if (fileSpecObj->isDict()) {
    if (!fileSpecObj->dictLookup("Unix", &obj1)->isString()) {
      obj1.free();
      fileSpecObj->dictLookup("F", &obj1);
    }
    if (obj1.isString())
      name = obj1.getString()->copy();
    else
      error(-1, "Illegal file spec in link");
    obj1.free();

  // error
  } else {
    error(-1, "Illegal file spec in link");
  }

  return name;
}
