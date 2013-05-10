//========================================================================
//
// FormWidget.cc
//
// Copyright 2000 Derek B. Noonburg
//
//========================================================================

#ifdef __GNUC__
#pragma implementation
#endif

#include "gmem.h"
#include "Object.h"
#include "Gfx.h"
#include "FormWidget.h"

//------------------------------------------------------------------------
// FormWidget
//------------------------------------------------------------------------

FormWidget::FormWidget(Dict *dict) {
  Object obj1, obj2;
  double t;

  ok = gFalse;

  if (dict->lookup("AP", &obj1)->isDict()) {
    obj1.dictLookupNF("N", &obj2);
    //~ this doesn't handle appearances with multiple states --
    //~ need to look at AS key to get state and then get the
    //~ corresponding entry from the N dict
    if (obj2.isRef()) {
      obj2.copy(&appearance);
      ok = gTrue;
    }
    obj2.free();
  }
  obj1.free();

  if (dict->lookup("Rect", &obj1)->isArray() &&
      obj1.arrayGetLength() == 4) {
    //~ should check object types here
    obj1.arrayGet(0, &obj2);
    xMin = obj2.getNum();
    obj2.free();
    obj1.arrayGet(1, &obj2);
    yMin = obj2.getNum();
    obj2.free();
    obj1.arrayGet(2, &obj2);
    xMax = obj2.getNum();
    obj2.free();
    obj1.arrayGet(3, &obj2);
    yMax = obj2.getNum();
    obj2.free();
    if (xMin > xMax) {
      t = xMin; xMin = xMax; xMax = t;
    }
    if (yMin > yMax) {
      t = yMin; yMin = yMax; yMax = t;
    }
  } else {
    //~ this should return an error
    xMin = yMin = 0;
    xMax = yMax = 1;
  }
  obj1.free();
}

FormWidget::~FormWidget() {
  appearance.free();
}

void FormWidget::draw(Gfx *gfx) {
  Object obj;

  if (appearance.fetch(&obj)->isStream()) {
    gfx->doWidgetForm(&obj, xMin, yMin, xMax, yMax);
  }
  obj.free();
}

//------------------------------------------------------------------------
// FormWidgets
//------------------------------------------------------------------------

FormWidgets::FormWidgets(Object *annots) {
  FormWidget *widget;
  Object obj1, obj2;
  int size;
  int i;

  widgets = NULL;
  size = 0;
  nWidgets = 0;

  if (annots->isArray()) {
    for (i = 0; i < annots->arrayGetLength(); ++i) {
      if (annots->arrayGet(i, &obj1)->isDict()) {
	obj1.dictLookup("Subtype", &obj2);
	if (obj2.isName("Widget") ||
	    obj2.isName("Stamp")) {
	  widget = new FormWidget(obj1.getDict());
	  if (widget->isOk()) {
	    if (nWidgets >= size) {
	      size += 16;
	      widgets = (FormWidget **)grealloc(widgets,
						size * sizeof(FormWidget *));
	    }
	    widgets[nWidgets++] = widget;
	  } else {
	    delete widget;
	  }
	}
	obj2.free();
      }
      obj1.free();
    }
  }
}

FormWidgets::~FormWidgets() {
  int i;

  for (i = 0; i < nWidgets; ++i) {
    delete widgets[i];
  }
  gfree(widgets);
}
