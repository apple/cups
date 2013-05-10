//========================================================================
//
// Page.cc
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
#include "XRef.h"
#include "Link.h"
#include "OutputDev.h"
#ifndef PDF_PARSER_ONLY
#include "Gfx.h"
#include "FormWidget.h"
#endif
#include "Error.h"

#include "Params.h"
#include "Page.h"

//------------------------------------------------------------------------
// PageAttrs
//------------------------------------------------------------------------

PageAttrs::PageAttrs(PageAttrs *attrs, Dict *dict) {
  Object obj1, obj2;
  double w, h;

  // get old/default values
  if (attrs) {
    x1 = attrs->x1;
    y1 = attrs->y1;
    x2 = attrs->x2;
    y2 = attrs->y2;
    cropX1 = attrs->cropX1;
    cropY1 = attrs->cropY1;
    cropX2 = attrs->cropX2;
    cropY2 = attrs->cropY2;
    rotate = attrs->rotate;
    attrs->resources.copy(&resources);
  } else {
    // set default MediaBox to 8.5" x 11" -- this shouldn't be necessary
    // but some (non-compliant) PDF files don't specify a MediaBox
    x1 = 0;
    y1 = 0;
    x2 = 612;
    y2 = 792;
    cropX1 = cropY1 = cropX2 = cropY2 = 0;
    rotate = 0;
    resources.initNull();
  }

  // media box
  dict->lookup("MediaBox", &obj1);
  if (obj1.isArray() && obj1.arrayGetLength() == 4) {
    obj1.arrayGet(0, &obj2);
    if (obj2.isNum())
      x1 = obj2.getNum();
    obj2.free();
    obj1.arrayGet(1, &obj2);
    if (obj2.isNum())
      y1 = obj2.getNum();
    obj2.free();
    obj1.arrayGet(2, &obj2);
    if (obj2.isNum())
      x2 = obj2.getNum();
    obj2.free();
    obj1.arrayGet(3, &obj2);
    if (obj2.isNum())
      y2 = obj2.getNum();
    obj2.free();
  }
  obj1.free();

  // crop box
  dict->lookup("CropBox", &obj1);
  if (obj1.isArray() && obj1.arrayGetLength() == 4) {
    obj1.arrayGet(0, &obj2);
    if (obj2.isNum())
      cropX1 = obj2.getNum();
    obj2.free();
    obj1.arrayGet(1, &obj2);
    if (obj2.isNum())
      cropY1 = obj2.getNum();
    obj2.free();
    obj1.arrayGet(2, &obj2);
    if (obj2.isNum())
      cropX2 = obj2.getNum();
    obj2.free();
    obj1.arrayGet(3, &obj2);
    if (obj2.isNum())
      cropY2 = obj2.getNum();
    obj2.free();
  }
  obj1.free();

  // if the MediaBox is excessively larger than the CropBox,
  // just use the CropBox
  limitToCropBox = gFalse;
  w = 0.25 * (cropX2 - cropX1);
  h = 0.25 * (cropY2 - cropY1);
  if (cropX2 > cropX1 &&
      ((cropX1 - x1) + (x2 - cropX2) > w ||
       (cropY1 - y1) + (y2 - cropY2) > h)) {
    limitToCropBox = gTrue;
  }

  // rotate
  dict->lookup("Rotate", &obj1);
  if (obj1.isInt())
    rotate = obj1.getInt();
  obj1.free();
  while (rotate < 0)
    rotate += 360;
  while (rotate >= 360)
    rotate -= 360;

  // resource dictionary
  dict->lookup("Resources", &obj1);
  if (obj1.isDict()) {
    resources.free();
    obj1.copy(&resources);
  }
  obj1.free();
}

PageAttrs::~PageAttrs() {
  resources.free();
}

//------------------------------------------------------------------------
// Page
//------------------------------------------------------------------------

Page::Page(int num1, Dict *pageDict, PageAttrs *attrs1) {

  ok = gTrue;
  num = num1;

  // get attributes
  attrs = attrs1;

  // annotations
  pageDict->lookupNF("Annots", &annots);
  if (!(annots.isRef() || annots.isArray() || annots.isNull())) {
    error(-1, "Page annotations object (page %d) is wrong type (%s)",
	  num, annots.getTypeName());
    annots.free();
    goto err2;
  }

  // contents
  pageDict->lookupNF("Contents", &contents);
  if (!(contents.isRef() || contents.isArray() ||
	contents.isNull())) {
    error(-1, "Page contents object (page %d) is wrong type (%s)",
	  num, contents.getTypeName());
    contents.free();
    goto err1;
  }

  return;

 err2:
  annots.initNull();
 err1:
  contents.initNull();
  ok = gFalse;
}

Page::~Page() {
  delete attrs;
  annots.free();
  contents.free();
}

void Page::display(OutputDev *out, double dpi, int rotate,
		   Links *links, Catalog *catalog) {
#ifndef PDF_PARSER_ONLY
  Gfx *gfx;
  Object obj;
  Link *link;
  int i;
  FormWidgets *formWidgets;

  if (printCommands) {
    printf("***** MediaBox = ll:%g,%g ur:%g,%g\n",
	   getX1(), getY1(), getX2(), getY2());
    if (isCropped()) {
      printf("***** CropBox = ll:%g,%g ur:%g,%g\n",
	     getCropX1(), getCropY1(), getCropX2(), getCropY2());
    }
    printf("***** Rotate = %d\n", attrs->getRotate());
  }

  rotate += getRotate();
  if (rotate >= 360) {
    rotate -= 360;
  } else if (rotate < 0) {
    rotate += 360;
  }
  gfx = new Gfx(out, num, attrs->getResourceDict(),
		dpi, getX1(), getY1(), getX2(), getY2(), isCropped(),
		getCropX1(), getCropY1(), getCropX2(), getCropY2(), rotate);
  contents.fetch(&obj);
  if (!obj.isNull()) {
    gfx->display(&obj);
  }
  obj.free();

  // draw links
  if (links) {
    for (i = 0; i < links->getNumLinks(); ++i) {
      link = links->getLink(i);
      out->drawLink(link, catalog);
    }
    out->dump();
  }

  // draw AcroForm widgets
  //~ need to reset CTM ???
  formWidgets = new FormWidgets(annots.fetch(&obj));
  obj.free();
  if (printCommands && formWidgets->getNumWidgets() > 0) {
    printf("***** AcroForm widgets\n");
  }
  for (i = 0; i < formWidgets->getNumWidgets(); ++i) {
    formWidgets->getWidget(i)->draw(gfx);
  }
  if (formWidgets->getNumWidgets() > 0) {
    out->dump();
  }
  delete formWidgets;

  delete gfx;
#endif
}
