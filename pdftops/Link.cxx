//========================================================================
//
// Link.cc
//
// Copyright 1996-2004 Glyph & Cog, LLC
//
//========================================================================

#include <config.h>

#ifdef USE_GCC_PRAGMAS
#pragma implementation
#endif

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
// LinkAction
//------------------------------------------------------------------------

LinkAction *LinkAction::parseDest(Object *obj) {
  LinkAction *action;

  action = new LinkGoTo(obj);
  if (!action->isOk()) {
    delete action;
    return NULL;
  }
  return action;
}

LinkAction *LinkAction::parseAction(Object *obj, GString *baseURI) {
  LinkAction *action;
  Object obj2, obj3, obj4;

  if (!obj->isDict()) {
    error(-1, "Bad annotation action");
    return NULL;
  }

  obj->dictLookup("S", &obj2);

  // GoTo action
  if (obj2.isName("GoTo")) {
    obj->dictLookup("D", &obj3);
    action = new LinkGoTo(&obj3);
    obj3.free();

  // GoToR action
  } else if (obj2.isName("GoToR")) {
    obj->dictLookup("F", &obj3);
    obj->dictLookup("D", &obj4);
    action = new LinkGoToR(&obj3, &obj4);
    obj3.free();
    obj4.free();

  // Launch action
  } else if (obj2.isName("Launch")) {
    action = new LinkLaunch(obj);

  // URI action
  } else if (obj2.isName("URI")) {
    obj->dictLookup("URI", &obj3);
    action = new LinkURI(&obj3, baseURI);
    obj3.free();

  // Named action
  } else if (obj2.isName("Named")) {
    obj->dictLookup("N", &obj3);
    action = new LinkNamed(&obj3);
    obj3.free();

  // Movie action
  } else if (obj2.isName("Movie")) {
    obj->dictLookupNF("Annot", &obj3);
    obj->dictLookup("T", &obj4);
    action = new LinkMovie(&obj3, &obj4);
    obj3.free();
    obj4.free();

  // unknown action
  } else if (obj2.isName()) {
    action = new LinkUnknown(obj2.getName());

  // action is missing or wrong type
  } else {
    error(-1, "Bad annotation action");
    action = NULL;
  }

  obj2.free();

  if (action && !action->isOk()) {
    delete action;
    return NULL;
  }
  return action;
}

GString *LinkAction::getFileSpecName(Object *fileSpecObj) {
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

//------------------------------------------------------------------------
// LinkDest
//------------------------------------------------------------------------

LinkDest::LinkDest(Array *a) {
  Object obj1, obj2;

  // initialize fields
  left = bottom = right = top = zoom = 0;
  ok = gFalse;

  // get page
  if (a->getLength() < 2) {
    error(-1, "Annotation destination array is too short");
    return;
  }
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
    if (a->getLength() < 3) {
      changeLeft = gFalse;
    } else {
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
    }
    if (a->getLength() < 4) {
      changeTop = gFalse;
    } else {
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
    }
    if (a->getLength() < 5) {
      changeZoom = gFalse;
    } else {
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
    }

  // Fit link
  } else if (obj1.isName("Fit")) {
    if (a->getLength() < 2) {
      error(-1, "Annotation destination array is too short");
      goto err2;
    }
    kind = destFit;

  // FitH link
  } else if (obj1.isName("FitH")) {
    if (a->getLength() < 3) {
      error(-1, "Annotation destination array is too short");
      goto err2;
    }
    kind = destFitH;
    if (!a->get(2, &obj2)->isNum()) {
      error(-1, "Bad annotation destination position");
      goto err1;
    }
    top = obj2.getNum();
    obj2.free();

  // FitV link
  } else if (obj1.isName("FitV")) {
    if (a->getLength() < 3) {
      error(-1, "Annotation destination array is too short");
      goto err2;
    }
    kind = destFitV;
    if (!a->get(2, &obj2)->isNum()) {
      error(-1, "Bad annotation destination position");
      goto err1;
    }
    left = obj2.getNum();
    obj2.free();

  // FitR link
  } else if (obj1.isName("FitR")) {
    if (a->getLength() < 6) {
      error(-1, "Annotation destination array is too short");
      goto err2;
    }
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
    if (a->getLength() < 2) {
      error(-1, "Annotation destination array is too short");
      goto err2;
    }
    kind = destFitB;

  // FitBH link
  } else if (obj1.isName("FitBH")) {
    if (a->getLength() < 3) {
      error(-1, "Annotation destination array is too short");
      goto err2;
    }
    kind = destFitBH;
    if (!a->get(2, &obj2)->isNum()) {
      error(-1, "Bad annotation destination position");
      goto err1;
    }
    top = obj2.getNum();
    obj2.free();

  // FitBV link
  } else if (obj1.isName("FitBV")) {
    if (a->getLength() < 3) {
      error(-1, "Annotation destination array is too short");
      goto err2;
    }
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
#ifdef WIN32
      if (actionObj->dictLookup("Win", &obj1)->isDict()) {
	obj1.dictLookup("F", &obj2);
	fileName = getFileSpecName(&obj2);
	obj2.free();
	if (obj1.dictLookup("P", &obj2)->isString()) {
	  params = obj2.getString()->copy();
	}
	obj2.free();
      } else {
	error(-1, "Bad launch-type link action");
      }
#else
      //~ This hasn't been defined by Adobe yet, so assume it looks
      //~ just like the Win dictionary until they say otherwise.
      if (actionObj->dictLookup("Unix", &obj1)->isDict()) {
	obj1.dictLookup("F", &obj2);
	fileName = getFileSpecName(&obj2);
	obj2.free();
	if (obj1.dictLookup("P", &obj2)->isString()) {
	  params = obj2.getString()->copy();
	}
	obj2.free();
      } else {
	error(-1, "Bad launch-type link action");
      }
#endif
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
// LinkMovie
//------------------------------------------------------------------------

LinkMovie::LinkMovie(Object *annotObj, Object *titleObj) {
  annotRef.num = -1;
  title = NULL;
  if (annotObj->isRef()) {
    annotRef = annotObj->getRef();
  } else if (titleObj->isString()) {
    title = titleObj->getString()->copy();
  } else {
    error(-1, "Movie action is missing both the Annot and T keys");
  }
}

LinkMovie::~LinkMovie() {
  if (title) {
    delete title;
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
// LinkBorderStyle
//------------------------------------------------------------------------

LinkBorderStyle::LinkBorderStyle(LinkBorderType typeA, double widthA,
				 double *dashA, int dashLengthA,
				 double rA, double gA, double bA) {
  type = typeA;
  width = widthA;
  dash = dashA;
  dashLength = dashLengthA;
  r = rA;
  g = gA;
  b = bA;
}

LinkBorderStyle::~LinkBorderStyle() {
  if (dash) {
    gfree(dash);
  }
}

//------------------------------------------------------------------------
// Link
//------------------------------------------------------------------------

Link::Link(Dict *dict, GString *baseURI) {
  Object obj1, obj2, obj3;
  LinkBorderType borderType;
  double borderWidth;
  double *borderDash;
  int borderDashLength;
  double borderR, borderG, borderB;
  double t;
  int i;

  borderStyle = NULL;
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

  // get the border style info
  borderType = linkBorderSolid;
  borderWidth = 1;
  borderDash = NULL;
  borderDashLength = 0;
  borderR = 0;
  borderG = 0;
  borderB = 1;
  if (dict->lookup("BS", &obj1)->isDict()) {
    if (obj1.dictLookup("S", &obj2)->isName()) {
      if (obj2.isName("S")) {
	borderType = linkBorderSolid;
      } else if (obj2.isName("D")) {
	borderType = linkBorderDashed;
      } else if (obj2.isName("B")) {
	borderType = linkBorderEmbossed;
      } else if (obj2.isName("I")) {
	borderType = linkBorderEngraved;
      } else if (obj2.isName("U")) {
	borderType = linkBorderUnderlined;
      }
    }
    obj2.free();
    if (obj1.dictLookup("W", &obj2)->isNum()) {
      borderWidth = obj2.getNum();
    }
    obj2.free();
    if (obj1.dictLookup("D", &obj2)->isArray()) {
      borderDashLength = obj2.arrayGetLength();
      borderDash = (double *)gmalloc(borderDashLength * sizeof(double));
      for (i = 0; i < borderDashLength; ++i) {
	if (obj2.arrayGet(i, &obj3)->isNum()) {
	  borderDash[i] = obj3.getNum();
	} else {
	  borderDash[i] = 1;
	}
	obj3.free();
      }
    }
    obj2.free();
  } else {
    obj1.free();
    if (dict->lookup("Border", &obj1)->isArray()) {
      if (obj1.arrayGetLength() >= 3) {
	if (obj1.arrayGet(2, &obj2)->isNum()) {
	  borderWidth = obj2.getNum();
	}
	obj2.free();
	if (obj1.arrayGetLength() >= 4) {
	  if (obj1.arrayGet(3, &obj2)->isArray()) {
	    borderType = linkBorderDashed;
	    borderDashLength = obj2.arrayGetLength();
	    borderDash = (double *)gmalloc(borderDashLength * sizeof(double));
	    for (i = 0; i < borderDashLength; ++i) {
	      if (obj2.arrayGet(i, &obj3)->isNum()) {
		borderDash[i] = obj3.getNum();
	      } else {
		borderDash[i] = 1;
	      }
	      obj3.free();
	    }
	  }
	  obj2.free();
	}
      }
    }
  }
  obj1.free();
  if (dict->lookup("C", &obj1)->isArray() && obj1.arrayGetLength() == 3) {
    if (obj1.arrayGet(0, &obj2)->isNum()) {
      borderR = obj2.getNum();
    }
    obj1.free();
    if (obj1.arrayGet(1, &obj2)->isNum()) {
      borderG = obj2.getNum();
    }
    obj1.free();
    if (obj1.arrayGet(2, &obj2)->isNum()) {
      borderB = obj2.getNum();
    }
    obj1.free();
  }
  obj1.free();
  borderStyle = new LinkBorderStyle(borderType, borderWidth,
				    borderDash, borderDashLength,
				    borderR, borderG, borderB);

  // look for destination
  if (!dict->lookup("Dest", &obj1)->isNull()) {
    action = LinkAction::parseDest(&obj1);

  // look for action
  } else {
    obj1.free();
    if (dict->lookup("A", &obj1)->isDict()) {
      action = LinkAction::parseAction(&obj1, baseURI);
    }
  }
  obj1.free();

  // check for bad action
  if (action) {
    ok = gTrue;
  }

  return;

 err1:
  obj2.free();
 err2:
  obj1.free();
}

Link::~Link() {
  if (borderStyle) {
    delete borderStyle;
  }
  if (action) {
    delete action;
  }
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
