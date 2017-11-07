//
// Attribute class for the CUPS PPD Compiler.
//
// Copyright 2007-2009 by Apple Inc.
// Copyright 2002-2005 by Easy Software Products.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more information.
//

//
// Include necessary headers...
//

#include "ppdc-private.h"


//
// 'ppdcAttr::ppdcAttr()' - Create an attribute.
//

ppdcAttr::ppdcAttr(const char *n,	// I - Name
                   const char *s,	// I - Spec string
		   const char *t,	// I - Human-readable text
		   const char *v,	// I - Value
		   bool       loc)	// I - Localize this attribute?
  : ppdcShared()
{
  PPDC_NEW;

  name        = new ppdcString(n);
  selector    = new ppdcString(s);
  text        = new ppdcString(t);
  value       = new ppdcString(v);
  localizable = loc;
}


//
// 'ppdcAttr::~ppdcAttr()' - Destroy an attribute.
//

ppdcAttr::~ppdcAttr()
{
  PPDC_DELETE;

  name->release();
  selector->release();
  text->release();
  value->release();
}
